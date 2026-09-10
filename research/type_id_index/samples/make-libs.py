#!/usr/bin/env python3
"""
Generate C++ shared libraries for testing type_info collection.

Each library contains:
- NC classes, in a mix of inheritance shapes
- An array of ND random numbers

The inheritance shape matters: the Itanium ABI emits a different type_info
record per shape, and the records have different sizes, so a library built
from a single shape has a uniform stride that is an artefact of the generator.

    kind      C++ shape                              ABI record                size
    root      no base                                __class_type_info         16
    single    one public non-virtual base at 0       __si_class_type_info      24
    virtual   one public *virtual* base              __vmi_class_type_info     40
    multi     k pairwise-unrelated public bases      __vmi_class_type_info  24+16k

Each class also gets a random number of virtual functions. That is not
cosmetic: vtables live in .data.rel.ro interleaved with the type_info records,
so their size is what dilutes the records and sets the realistic density.
"""

import argparse
import os
import random
import sys
from collections import namedtuple
from pathlib import Path

ClassSpec = namedtuple("ClassSpec", "index kind bases methods")

KINDS = ("root", "single", "virtual", "multi")


def record_size(spec):
    """Expected size of the type_info record for a class, in bytes."""
    if spec.kind == "root":
        return 16
    if spec.kind == "single":
        return 24
    return 24 + 16 * len(spec.bases)


def parse_range(text):
    """Parse 'K' or 'K-L' into (K, L)."""
    if "-" in text:
        lo, hi = text.split("-", 1)
        return int(lo), int(hi)
    value = int(text)
    return value, value


def plan_classes(num_classes, shape, rng):
    """Plan the inheritance shape of every class in one library.

    Returns a list of ClassSpec in declaration order; a class's bases always
    have a lower index, so declaring them in order is enough.
    """
    fractions = [shape[f"frac_{kind}"] for kind in KINDS]
    total = sum(fractions)

    if total <= 0:
        raise ValueError("the --frac-* options must not all be zero")

    cumulative = []
    running = 0.0

    for fraction in fractions:
        running += fraction / total
        cumulative.append(running)

    specs = []
    ancestors = []  # ancestors[i]: transitive base indices of class i
    depth = []

    for index in range(num_classes):
        draw = rng.random()
        kind = KINDS[-1]

        for candidate, bound in zip(KINDS, cumulative):
            if draw < bound:
                kind = candidate
                break

        window = shape["pool_window"]
        pool = range(max(0, index - window) if window else 0, index)
        candidates = [i for i in pool if depth[i] + 1 <= shape["max_depth"]]

        if not candidates:
            kind = "root"

        bases = []

        if kind in ("single", "virtual"):
            bases = [rng.choice(candidates)]
        elif kind == "multi":
            lo, hi = shape["multi_bases"]
            wanted = rng.randint(lo, hi)
            bases = pick_unrelated(candidates, wanted, ancestors, rng,
                                   shape["multi_tries"])

            if len(bases) < 2:
                # Not enough unrelated bases in the window: degrade.
                kind = "single"
                bases = [rng.choice(candidates)]

        specs.append(ClassSpec(
            index=index,
            kind=kind,
            bases=bases,
            methods=rng.randint(1, shape["methods_per_class"]),
        ))

        own_ancestors = set(bases)

        for base in bases:
            own_ancestors |= ancestors[base]

        ancestors.append(own_ancestors)
        depth.append(1 + max((depth[base] for base in bases), default=-1))

    return specs


def pick_unrelated(candidates, wanted, ancestors, rng, tries):
    """Pick up to `wanted` pairwise-unrelated indices out of `candidates`.

    Unrelated means neither is an ancestor of the other, which keeps the
    generated hierarchy free of ambiguous-base and dominance surprises.
    """
    if len(candidates) < 2:
        return []

    best = []

    for _ in range(tries):
        picked = []

        for candidate in rng.sample(candidates, min(len(candidates), wanted * 4)):
            if all(candidate not in ancestors[other]
                   and other not in ancestors[candidate]
                   for other in picked):
                picked.append(candidate)

            if len(picked) == wanted:
                break

        if len(picked) > len(best):
            best = picked

        if len(best) == wanted:
            break

    return best


def generate_random_numbers(n, rng):
    """Generate n random numbers."""
    if n <= 0:
        return []

    return [rng.randint(0, 1000000) for _ in range(n)]


def class_name(lib_index, index):
    return f"Lib{lib_index}Class{index}"


def generate_class(lib_index, spec):
    """Generate the definition of one class."""
    name = class_name(lib_index, spec.index)

    if spec.kind == "root":
        base_clause = ""
    else:
        keyword = "public virtual" if spec.kind == "virtual" else "public"
        base_clause = " : " + ", ".join(
            f"{keyword} {class_name(lib_index, base)}" for base in spec.bases
        )

    lines = [f"class {name}{base_clause} {{", "public:"]

    if spec.kind == "root":
        lines.append(f"    virtual ~{name}() = default;")

    # The first method is declared here and defined below, out of line: it is
    # the class's *key function*, so the vtable and the type_info are emitted
    # in this translation unit as ordinary .data.rel.ro objects rather than as
    # COMDAT sections. That is what real libraries look like, and it is what
    # makes the linker interleave the two - all-inline classes put every
    # type_info in a .data.rel.ro._ZTI* section, and the linker's name sort
    # then packs the records back to back, an artefact that inflates density.
    lines.append(f"    virtual int m{spec.index}_0() const;")

    # Uniquely named methods: two unrelated bases must not contribute
    # same-signature virtuals, which would make a call ambiguous.
    for k in range(1, spec.methods):
        lines.append(
            f"    virtual int m{spec.index}_{k}() const {{ return value_ + {k}; }}")

    lines.append("")
    lines.append("private:")
    lines.append(f"    int value_ = {spec.index};")
    lines.append("};")

    return "\n".join(lines) + "\n"


def generate_library_files(lib_index, specs, random_numbers, classes_per_tu):
    """Generate the source files of one library, as {filename: content}.

    A library is a header holding every class definition plus one translation
    unit per slice of `classes_per_tu` classes, each defining the key functions
    of its slice. That shape matters: within a translation unit the compiler
    emits all the type_info objects together and all the vtables after them, so
    a single-TU library packs its records back to back - 100 % occupancy, an
    artefact. With many TUs each one contributes its own type_infos followed by
    its own vtables, so the records end up diluted by the vtables in between,
    which is what real libraries look like.
    """
    files = {}

    header = f"lib{lib_index}.hpp"
    files[header] = (
        f"// Auto-generated library {lib_index}: {len(specs)} classes\n"
        "#pragma once\n\n#include <cstddef>\n\n"
        + "\n".join(generate_class(lib_index, spec) for spec in specs)
    )

    parts = []

    for part, start in enumerate(range(0, len(specs), classes_per_tu)):
        slice_ = specs[start:start + classes_per_tu]
        name = f"lib{lib_index}_part{part}.cpp"
        parts.append(name)
        files[name] = (
            f'#include "{header}"\n\n'
            + "\n".join(
                f"int {class_name(lib_index, spec.index)}"
                f"::m{spec.index}_0() const {{ return value_; }}"
                for spec in slice_
            )
            + "\n"
        )

    files[f"lib{lib_index}.cpp"] = generate_main_tu(
        lib_index, specs, random_numbers, header)

    return files, parts


def generate_main_tu(lib_index, specs, random_numbers, header):
    """Generate the translation unit holding the data array and the exports."""

    random_values = ", ".join(str(f) for f in random_numbers)
    random_code = f"""
// Random numbers array
const long long random_numbers_{lib_index}[{len(random_numbers)}] = {{
    {random_values}
}};
"""

    # Factory functions, so that no class is dead-stripped.
    factory_code = f"""
// Factory function to create instances
void* createInstance_{lib_index}(int classIndex) {{
    switch(classIndex) {{
"""
    for spec in specs:
        name = class_name(lib_index, spec.index)
        factory_code += f"        case {spec.index}: return new {name}();\n"

    factory_code += f"""        default: return nullptr;
    }}
}}

// Cleanup function
void deleteInstance_{lib_index}(void* ptr, int classIndex) {{
    if (!ptr) return;
    switch(classIndex) {{
"""
    for spec in specs:
        name = class_name(lib_index, spec.index)
        factory_code += (
            f"        case {spec.index}: delete static_cast<{name}*>(ptr); break;\n"
        )

    factory_code += """        default: break;
    }
}
"""

    num_classes = len(specs)

    return f"""// Auto-generated library {lib_index}
// Contains {num_classes} classes and {len(random_numbers)} random numbers

#include "{header}"

{random_code}

{factory_code}

// Export symbols
extern "C" {{
    void* lib{lib_index}_create(int classIndex) {{
        return createInstance_{lib_index}(classIndex);
    }}

    void lib{lib_index}_delete(void* ptr, int classIndex) {{
        deleteInstance_{lib_index}(ptr, classIndex);
    }}

    const long long* lib{lib_index}_get_random_numbers() {{
        return random_numbers_{lib_index};
    }}

    int lib{lib_index}_get_random_numbers_count() {{
        return {len(random_numbers)};
    }}

    int lib{lib_index}_get_class_count() {{
        return {num_classes};
    }}
}}
"""


def generate_cmake(sources):
    """Generate CMakeLists.txt for building all libraries.

    `sources` maps a library index to its list of source files.
    """

    cmake_code = """cmake_minimum_required(VERSION 3.15)
project(TypeInfoTestLibs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Generate shared libraries
"""

    for i, files in sorted(sources.items()):
        cmake_code += f"""
add_library(testlib{i} SHARED {' '.join(files)})
set_target_properties(testlib{i} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/libs"
    RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/libs"
)
"""

    return cmake_code


def print_shape(lib_index, specs):
    """Report the planned kind and record-size histogram."""
    kinds = {}
    sizes = {}

    for spec in specs:
        kinds[spec.kind] = kinds.get(spec.kind, 0) + 1
        size = record_size(spec)
        sizes[size] = sizes.get(size, 0) + 1

    total_bytes = sum(record_size(spec) for spec in specs)
    methods = sum(spec.methods for spec in specs)

    print(f"  lib{lib_index}: "
          + ", ".join(f"{kind}={kinds.get(kind, 0)}" for kind in KINDS))
    print("    record sizes: "
          + ", ".join(f"{size}:{count}" for size, count in sorted(sizes.items())))
    print(f"    sum of record sizes: {total_bytes} bytes; "
          f"virtual functions: {methods}")


def main():
    parser = argparse.ArgumentParser(
        description="Generate C++ shared libraries for type_info testing"
    )
    parser.add_argument("NL", type=int, help="Number of libraries to generate")
    parser.add_argument("NC", type=int, help="Number of classes per library")
    parser.add_argument("ND", type=int, help="Number of random numbers")
    parser.add_argument(
        "-o",
        "--output-dir",
        default="generated_libs",
        help="Output directory for generated files (default: generated_libs)",
    )
    parser.add_argument(
        "-B",
        "--build",
        action="store_true",
        help="Build the libraries after generating",
    )
    parser.add_argument("--seed", type=int, default=20260906, help="RNG seed")
    parser.add_argument("--frac-root", type=float, default=0.10,
                        help="Fraction of classes with no base (16 B records)")
    parser.add_argument("--frac-single", type=float, default=0.55,
                        help="Fraction with one non-virtual base (24 B)")
    parser.add_argument("--frac-virtual", type=float, default=0.15,
                        help="Fraction with one virtual base (40 B)")
    parser.add_argument("--frac-multi", type=float, default=0.20,
                        help="Fraction with several bases (24+16k B)")
    parser.add_argument("--multi-bases", default="2-3",
                        help="Number of bases for 'multi' classes, K or K-L")
    parser.add_argument("--multi-tries", type=int, default=20,
                        help="Attempts at finding pairwise-unrelated bases")
    parser.add_argument("--max-depth", type=int, default=6,
                        help="Maximum inheritance depth")
    parser.add_argument("--pool-window", type=int, default=64,
                        help="Draw bases from the last W classes (0 = all)")
    parser.add_argument("--methods-per-class", type=int, default=6,
                        help="Each class gets 1..M virtual functions")
    parser.add_argument("--classes-per-tu", type=int, default=25,
                        help="Classes whose key functions share a translation "
                             "unit (a single TU packs the type_info records "
                             "back to back, which is an artefact)")
    parser.add_argument("--print-shape", action="store_true",
                        help="Report the planned kind/record-size histogram")

    args = parser.parse_args()

    if args.NL <= 0 or args.NC <= 0 or args.ND <= 0:
        print("Error: NL, NC, and ND must be positive integers", file=sys.stderr)
        return 1

    if args.ND > 10000:
        print(f"Warning: ND={args.ND} produces a large rodata array; the gaps "
              "between the libraries' type_info records will be dominated by "
              "it rather than by realistic library layout.", file=sys.stderr)

    shape = {
        "frac_root": args.frac_root,
        "frac_single": args.frac_single,
        "frac_virtual": args.frac_virtual,
        "frac_multi": args.frac_multi,
        "multi_bases": parse_range(args.multi_bases),
        "multi_tries": args.multi_tries,
        "max_depth": args.max_depth,
        "pool_window": args.pool_window,
        "methods_per_class": args.methods_per_class,
    }

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(
        f"Generating {args.NL} libraries with {args.NC} classes each and {args.ND} random numbers..."
    )

    random_numbers = generate_random_numbers(args.ND, random.Random(args.seed))

    sources = {}

    for i in range(args.NL):
        specs = plan_classes(args.NC, shape, random.Random(args.seed + i))
        files, parts = generate_library_files(
            i, specs, random_numbers, args.classes_per_tu)

        for name, content in files.items():
            with open(output_dir / name, "w") as f:
                f.write(content)

        sources[i] = [f"lib{i}.cpp"] + parts
        print(f"Generated lib{i}.hpp, lib{i}.cpp and {len(parts)} part files")

        if args.print_shape:
            print_shape(i, specs)

    cmake_code = generate_cmake(sources)
    cmake_file = output_dir / "CMakeLists.txt"
    with open(cmake_file, "w") as f:
        f.write(cmake_code)
    print(f"Generated {cmake_file}")

    if args.build:
        print("\nBuilding libraries...")
        build_dir = output_dir / "build"
        build_dir.mkdir(exist_ok=True)

        import subprocess

        result = subprocess.run(
            ["cmake", ".."], cwd=build_dir, capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"CMake configuration failed:\n{result.stderr}", file=sys.stderr)
            return 1

        result = subprocess.run(
            ["cmake", "--build", ".", f"-j{os.cpu_count() or 1}"],
            cwd=build_dir, capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"Build failed:\n{result.stderr}", file=sys.stderr)
            return 1

        print(f"Libraries built successfully in {build_dir / 'libs'}")

    print("\nDone!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
