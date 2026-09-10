#!/usr/bin/env python3
"""
Analyse type_info addresses for boostorg/openmethod#57.

The question: can a type_id (a `const std::type_info*`) be mapped to a v-table
table index by `(ti - base) / d`, instead of the multiplicative perfect hash
`(mult * ti) >> shift` that policies::fast_perfect_hash searches for?

Injectivity is not in question. Every Itanium ABI type_info record is at least
16 bytes (__class_type_info is 16, __si_class_type_info 24, __vmi_class_type_info
24+16k) and distinct objects do not overlap, so two distinct type ids differ by
at least 16; and a - b >= d implies a//d > b//d. The map is therefore injective
for every d <= 16, with no search and no failure mode - except memory.

Memory is what this script measures: the *span* of the registered type ids
divided by d is the number of slots, against fast_perfect_hash's 1.25n..2.5n.

Inputs are the JSON files written by build/collect-type-infos (runtime
addresses, post-relocation, as &typeid(X) would yield), joined with `readelf`
for each record's size and each module's load base.
"""

import argparse
import functools
import glob
import json
import math
import os
import statistics
import subprocess
import sys
from collections import defaultdict

PAGE = 4096

# Measured on this machine from /proc/self/maps: a PIE executable and the
# shared libraries land in different regions of the address space.
PIE_BASE = 0x5B736A392000
LIB_BASE = 0x7B0BB7400000


# ---------------------------------------------------------------- ELF join


@functools.lru_cache(maxsize=None)
def read_elf_type_infos(library):
    """{demangled name: (st_value, st_size)} for the _ZTI* symbols of a library."""
    out = subprocess.run(
        ["readelf", "-W", "--dyn-syms", library],
        capture_output=True, text=True, check=True).stdout

    mangled = {}

    for line in out.splitlines():
        fields = line.split()

        if len(fields) >= 8 and fields[7].startswith("_ZTI") and fields[6] != "UND":
            mangled[fields[7]] = (int(fields[1], 16), int(fields[2]))

    if not mangled:
        return {}

    names = list(mangled)
    demangled = subprocess.run(
        ["c++filt"], input="\n".join(names), capture_output=True,
        text=True, check=True).stdout.splitlines()

    return {d: mangled[m] for m, d in zip(names, demangled)}


def load_sample(path):
    """Read one collect-type-infos file and join it with the ELF data.

    Returns (records, modules) where records is a list of
    (address, size, module index) and modules is the list of library paths.
    """
    with open(path) as f:
        data = json.load(f)

    libraries = data["libraries"]
    by_name = {}

    for index, library in enumerate(libraries):
        for name, (value, size) in read_elf_type_infos(library).items():
            if name in by_name:
                sys.exit(f"{path}: name {name!r} defined by two modules; the "
                         "join is ambiguous, extend the collector instead")

            by_name[name] = (index, value, size)

    records = []
    bases = {}

    for entry in data["typeinfos"]:
        address = int(entry["address"], 16)
        found = by_name.get(entry["name"])

        if found is None:
            continue

        index, value, size = found
        base = address - value
        bases.setdefault(index, base)

        if bases[index] != base:
            sys.exit(f"{path}: module {libraries[index]} has two load bases "
                     f"({bases[index]:#x}, {base:#x}); a symbol was interposed")

        records.append((address, size, index))

    for index, base in bases.items():
        if base % PAGE:
            sys.exit(f"{path}: load base {base:#x} is not page-aligned")

    records.sort()

    return records, libraries


# ---------------------------------------------------------------- placement


def module_extent(records):
    """{module index: (lowest address, highest address + size)}."""
    extent = {}

    for address, size, index in records:
        lo, hi = extent.get(index, (address, address + size))
        extent[index] = (min(lo, address), max(hi, address + size))

    return extent


def replace_bases(records, new_base):
    """Re-address records, moving each module to new_base[module index]."""
    extent = module_extent(records)
    moved = []

    for address, size, index in records:
        moved.append(
            (address - extent[index][0] + new_base[index], size, index))

    moved.sort()

    return moved


def place(records, model):
    """Apply a placement model to a sample's records."""
    if model == "observed":
        return records

    extent = module_extent(records)
    order = sorted(extent)

    if model == "adjacent":
        # What the loader does for libraries mapped back to back.
        bases = {}
        cursor = LIB_BASE

        for index in order:
            bases[index] = cursor
            span = extent[index][1] - extent[index][0]
            cursor += (span + PAGE - 1) // PAGE * PAGE

        return replace_bases(records, bases)

    if model == "pie-split":
        # One module is the program, the rest are shared libraries. This is the
        # ordinary case for a program that registers classes of its own.
        bases = {order[0]: PIE_BASE}
        cursor = LIB_BASE

        for index in order[1:]:
            bases[index] = cursor
            span = extent[index][1] - extent[index][0]
            cursor += (span + PAGE - 1) // PAGE * PAGE

        return replace_bases(records, bases)

    sys.exit(f"unknown placement model {model!r}")


# ---------------------------------------------------------------- subsets


def subset(records, mode, fraction, clusters, rng):
    """Model the fact that a program registers only *some* of the classes."""
    n = len(records)
    wanted = max(1, math.ceil(fraction * n))

    if mode == "all" or wanted >= n:
        return records

    if mode == "random":
        return sorted(rng.sample(records, wanted))

    if mode == "contiguous":
        start = rng.randrange(0, n - wanted + 1)

        return records[start:start + wanted]

    if mode == "clustered":
        per = max(1, wanted // clusters)
        picked = set()

        while len(picked) < wanted:
            start = rng.randrange(0, max(1, n - per))
            picked.update(range(start, min(n, start + per)))

        return [records[i] for i in sorted(picked)][:wanted]

    if mode == "per-module":
        # Restrict to a subset of the modules, then cluster inside them.
        modules = sorted({index for _, _, index in records})
        keep = set(rng.sample(modules, max(1, len(modules) // 2)))
        inside = [r for r in records if r[2] in keep]

        return subset(inside, "clustered", fraction, clusters, rng)

    sys.exit(f"unknown subset mode {mode!r}")


# ---------------------------------------------------------------- metrics


def largest_injective_divisor(addresses):
    """Largest power of two d for which (a - min) // d is injective."""
    gaps = min(b - a for a, b in zip(addresses, addresses[1:]))
    divisor = 1

    while divisor * 2 <= gaps:
        divisor *= 2

    return divisor


def fph_buckets(n):
    """The bucket count fast_perfect_hash starts from, as it computes it."""
    size = n * 5 // 4

    return 1 << max(1, size.bit_length())


def directory_bits(addresses):
    """Smallest k for which the low k bits of the page number are unique."""
    pages = {address >> 12 for address in addresses}

    for k in range(0, 25):
        if len({page & ((1 << k) - 1) for page in pages}) == len(pages):
            return k, len(pages)

    return None, len(pages)


def metrics(records, divisors):
    addresses = [address for address, _, _ in records]
    n = len(addresses)
    span = addresses[-1] - addresses[0]
    modules = sorted({index for _, _, index in records})

    row = {
        "n": n,
        "modules": len(modules),
        "span": span,
        "gcd": functools.reduce(math.gcd, [a - addresses[0] for a in addresses])
               if n > 1 else 0,
        "d_max": largest_injective_divisor(addresses) if n > 1 else 16,
        "occupancy": sum(size for _, size, _ in records) / span if span else 1.0,
        "fph_buckets": fph_buckets(n),
        "fph_bytes": 8 * fph_buckets(n),
    }

    for d in divisors:
        slots = span // d + 1
        row[f"slots@{d}"] = slots
        row[f"density@{d}"] = n / slots
        row[f"bytes@{d}"] = 8 * slots
        row[f"blowup@{d}"] = 8 * slots / row["fph_bytes"]
        row[f"bytes_per_class@{d}"] = 8 * slots / n

    bits, pages = directory_bits(addresses)
    row["pages"] = pages
    row["dir_bits"] = bits if bits is not None else -1
    # A page directory plus one 512-entry leaf per occupied page.
    row["two_level_bytes"] = (
        8 * (1 << bits) + pages * 512 * 8 if bits is not None else 0)

    extent = module_extent(records)
    intra = [hi - lo for lo, hi in extent.values()]
    ordered = sorted(extent.values())
    gaps = [b[0] - a[1] for a, b in zip(ordered, ordered[1:])]
    row["intra_span_max"] = max(intra)
    row["inter_gap_max"] = max(gaps) if gaps else 0

    return row


# ---------------------------------------------------------------- reporting


def percentile(values, q):
    values = sorted(values)

    if not values:
        return 0.0

    index = min(len(values) - 1, int(q * (len(values) - 1)))

    return values[index]


def human(value):
    for unit, scale in (("TB", 1 << 40), ("GB", 1 << 30), ("MB", 1 << 20),
                        ("KB", 1 << 10)):
        if value >= scale:
            return f"{value / scale:.1f} {unit}"

    return f"{value} B"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples-root", default="samples")
    parser.add_argument("--divisors", default="8,16,32,64")
    parser.add_argument("--subset-modes",
                        default="all,random,contiguous,clustered,per-module")
    parser.add_argument("--subset-fractions", default="1.0,0.5,0.25,0.1,0.05")
    parser.add_argument("--clusters", type=int, default=4)
    parser.add_argument("--samples", type=int, default=25,
                        help="replicates per subset model")
    parser.add_argument("--placement", default="observed,adjacent,pie-split")
    parser.add_argument("--seed", type=int, default=20260906)
    parser.add_argument("--csv", default="type-ids.csv")
    parser.add_argument("--markdown", default="type-ids.md")
    parser.add_argument("--export-type-ids",
                        help="write hex address files for bench_initialize")
    parser.add_argument("--representative", default="si-heavy",
                        help="configuration used for the subset sweep table")

    args = parser.parse_args()

    import random

    divisors = [int(d) for d in args.divisors.split(",")]
    modes = args.subset_modes.split(",")
    fractions = [float(f) for f in args.subset_fractions.split(",")]
    placements = args.placement.split(",")

    configs = sorted(
        d for d in os.listdir(args.samples_root)
        if os.path.isdir(os.path.join(args.samples_root, d)))

    rows = []

    for config in configs:
        pattern = os.path.join(args.samples_root, config, "runs", "*", "*.json")

        for path in sorted(glob.glob(pattern)):
            replicate = os.path.basename(os.path.dirname(path))
            combo = os.path.splitext(os.path.basename(path))[0]
            records, _ = load_sample(path)

            if len(records) < 2:
                continue

            for model in placements:
                placed = place(records, model)

                for mode in modes:
                    for fraction in (
                            [1.0] if mode == "all" else fractions):
                        replicates = 1 if mode in ("all",) else args.samples
                        rng = random.Random(
                            args.seed + hash((config, combo, mode)) % 100000)

                        for draw in range(replicates):
                            chosen = subset(
                                placed, mode, fraction, args.clusters, rng)

                            if len(chosen) < 2:
                                continue

                            row = metrics(chosen, divisors)
                            row.update(config=config, replicate=replicate,
                                       combo=combo, placement=model,
                                       mode=mode, fraction=fraction, draw=draw)
                            rows.append(row)

    if not rows:
        sys.exit(f"no samples found under {args.samples_root}/*/runs/")

    fields = ["config", "replicate", "combo", "placement", "mode", "fraction",
              "draw"] + [k for k in rows[0] if k not in
                         ("config", "replicate", "combo", "placement", "mode",
                          "fraction", "draw")]

    import csv

    with open(args.csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    write_markdown(args, rows, divisors)

    if args.export_type_ids:
        export_type_ids(args, configs)

    print(f"{len(rows)} rows -> {args.csv}, tables -> {args.markdown}")


def select(rows, **kwargs):
    return [r for r in rows
            if all(r[k] == v for k, v in kwargs.items())]


def write_markdown(args, rows, divisors):
    out = []

    # T1: one row per configuration, all classes, as observed, largest combo.
    out.append("### T1 - all registered classes, observed placement\n")
    out.append("| config | modules | N | span | GCD | d_max | occupancy | "
               "slots@8 | density@8 | table@8 | table@16 | fph table | "
               "blowup@8 |")
    out.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|")

    for config in sorted({r["config"] for r in rows}):
        candidates = select(rows, config=config, placement="observed",
                            mode="all")

        if not candidates:
            continue

        row = max(candidates, key=lambda r: (r["modules"], r["n"]))
        out.append(
            f"| {config} | {row['modules']} | {row['n']} | {human(row['span'])} "
            f"| {row['gcd']} | {row['d_max']} | {100 * row['occupancy']:.1f}% "
            f"| {row['slots@8']} | {100 * row['density@8']:.2f}% "
            f"| {human(row['bytes@8'])} | {human(row['bytes@16'])} "
            f"| {human(row['fph_bytes'])} | {row['blowup@8']:.0f}x |")

    # T2: subset sweep on the representative configuration.
    out.append(f"\n### T2 - subset models, {args.representative}, observed "
               "placement (median, p95)\n")
    out.append("| mode | fraction | N | span | slots@8 | density@8 | table@8 | "
               "blowup@8 |")
    out.append("|---|---|---|---|---|---|---|---|")

    for mode in args.subset_modes.split(","):
        for fraction in ([1.0] if mode == "all"
                         else [float(f) for f in
                               args.subset_fractions.split(",")]):
            group = select(rows, config=args.representative,
                           placement="observed", mode=mode, fraction=fraction)

            if not group:
                continue

            def med(key):
                return statistics.median(r[key] for r in group)

            def p95(key):
                return percentile([r[key] for r in group], 0.95)

            out.append(
                f"| {mode} | {fraction} | {med('n'):.0f} "
                f"| {human(int(med('span')))} | {med('slots@8'):.0f} "
                f"| {100 * med('density@8'):.2f}% "
                f"| {human(int(med('bytes@8')))} "
                f"| {med('blowup@8'):.0f}x (p95 {p95('blowup@8'):.0f}x) |")

    # T3: placement models - the decisive table.
    out.append(f"\n### T3 - placement models, {args.representative}, all "
               "classes\n")
    out.append("| placement | modules | span | slots@8 | table@8 | "
               "vs fast_perfect_hash |")
    out.append("|---|---|---|---|---|---|")

    for model in args.placement.split(","):
        group = select(rows, config=args.representative, placement=model,
                       mode="all")
        group = [r for r in group if r["modules"] > 1]

        if not group:
            continue

        row = max(group, key=lambda r: (r["modules"], r["n"]))
        out.append(
            f"| {model} | {row['modules']} | {human(row['span'])} "
            f"| {row['slots@8']:,} | {human(row['bytes@8'])} "
            f"| {row['blowup@8']:.0f}x |")

    # T4: the two-level refinement, on the same data.
    out.append(f"\n### T4 - page-directory refinement (R3)\n")
    out.append("| config | N | pages | dir_bits | two-level table | flat "
               "table@8 | fph table |")
    out.append("|---|---|---|---|---|---|---|")

    for config in sorted({r["config"] for r in rows}):
        candidates = select(rows, config=config, placement="observed",
                            mode="all")

        if not candidates:
            continue

        row = max(candidates, key=lambda r: (r["modules"], r["n"]))
        out.append(
            f"| {config} | {row['n']} | {row['pages']} | {row['dir_bits']} "
            f"| {human(row['two_level_bytes'])} | {human(row['bytes@8'])} "
            f"| {human(row['fph_bytes'])} |")

    with open(args.markdown, "w") as f:
        f.write("\n".join(out) + "\n")


def export_type_ids(args, configs):
    os.makedirs(args.export_type_ids, exist_ok=True)

    for config in configs:
        for path in sorted(glob.glob(os.path.join(
                args.samples_root, config, "runs", "0", "*.json"))):
            records, _ = load_sample(path)

            if len(records) < 2:
                continue

            combo = os.path.splitext(os.path.basename(path))[0]
            stem = f"{config}-{combo}-{len(records)}"
            modules = len({index for _, _, index in records})

            # The observed placement keeps the bare name; the modelled ones
            # are suffixed, so a run adds files rather than replacing any.
            models = ["observed"]

            if modules > 1:
                models += [m for m in args.placement.split(",")
                           if m != "observed"]

            for model in models:
                moved = place(records, model)
                suffix = "" if model == "observed" else f"-{model}"

                with open(os.path.join(
                        args.export_type_ids, stem + suffix + ".txt"),
                        "w") as f:
                    for address, _, _ in moved:
                        f.write(f"{address:x}\n")


if __name__ == "__main__":
    main()
