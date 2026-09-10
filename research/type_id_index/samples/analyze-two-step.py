#!/usr/bin/env python3
"""
Two-step index analysis for boostorg/openmethod#57 - see TYPE_ID_INDEX.md §13.

The flat (ti - base) / d table dies on the span *between* modules: an
executable and a dlopen'ed plugin are tens of terabytes apart. But the module
an address belongs to is written in its high bits, and those can be hashed
like anything else. So use two steps:

    group = (x >> K) * M >> S       # which module
    index = dir[group] + (x >> 3)   # that module's base, plus the flat index

The second level is the flat index applied once per module rather than once
per program, so the 16-byte ABI separation still makes it injective, and the
inter-module span is never represented at all. Folding the table's address
and the module's minimum into dir[group] recovers the fused load.

This script measures what that costs:

  flat@8       the single-table baseline, for reference
  2step@8      sum over modules of span/8 + 1 slots, at 8 bytes each
  2step@16     the same at d = 16, which forfeits the fused load
  dir          distinct (x >> K) values, i.e. directory entries, and whether
               any group is owned by two modules - which the scheme cannot
               represent, so a K that reports SHARED is unusable

The result: the directory is negligible and the second level is not. The
2step@8 column is the sum of the per-module spans, which is exactly what the
single-module rows of T1 already measured - so the scheme removes the
multi-module problem and keeps the within-module one, which was the expensive
half all along.
"""

import argparse
import glob
import importlib.util
import os
import sys


def load_analyzer():
    """Reuse load_sample() and place() from the address analysis."""
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "analyze-type-ids.py")
    spec = importlib.util.spec_from_file_location("analyze_type_ids", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    return module


def module_extents(records):
    extents = {}

    for address, _, index in records:
        low, high = extents.get(index, (address, address))
        extents[index] = (min(low, address), max(high, address))

    return extents


def two_step_slots(records, divisor):
    """Slots the second level needs: one flat table per module."""
    return sum((high - low) // divisor + 1
               for low, high in module_extents(records).values())


def directory(records, k):
    """(entries, groups owned by more than one module) for a key of x >> k."""
    owner = {}
    shared = set()

    for address, _, index in records:
        group = address >> k

        if owner.setdefault(group, index) != index:
            shared.add(group)

    return len(owner), len(shared)


def human(byte_count):
    if byte_count < 1024:
        return f"{byte_count} B"

    if byte_count < 1024 * 1024:
        return f"{byte_count / 1024:.0f} KB"

    if byte_count < 1024 * 1024 * 1024:
        return f"{byte_count / (1024 * 1024):.1f} MB"

    return f"{byte_count / (1024 ** 4):.1f} TB"


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--samples-root", default="samples")
    parser.add_argument("--combo", default="00000014",
                        help="which library combination (default: all four)")
    parser.add_argument("--placement", default="observed,pie-split")
    parser.add_argument("--keys", default="12,16,20,24",
                        help="values of K to report a directory size for")
    parser.add_argument("--markdown", default="two-step.md")

    args = parser.parse_args()
    analyzer = load_analyzer()
    keys = [int(k) for k in args.keys.split(",")]

    pattern = os.path.join(
        args.samples_root, "*", "runs", "0", f"{args.combo}.json")
    paths = sorted(glob.glob(pattern))

    if not paths:
        sys.exit(f"no samples matched {pattern}")

    out = ["### Two-step index - per-module directory plus a flat second level",
           "",
           f"Combination `{args.combo}`. `dir@K` is the directory size in "
           "entries; `SHARED` means two modules fall in one group at that K, "
           "which the scheme cannot represent.",
           "",
           "| config | placement | modules | N | flat@8 | 2step@8 | 2step@16 |"
           + "".join(f" dir@K={k} |" for k in keys),
           "|---|---|---|---|---|---|---|" + "---|" * len(keys)]

    for path in paths:
        config = path.split(os.sep)[-4]
        records, _ = analyzer.load_sample(path)

        for model in args.placement.split(","):
            moved = analyzer.place(records, model)
            modules = len(module_extents(moved))

            if model != "observed" and modules == 1:
                continue

            addresses = [address for address, _, _ in moved]
            flat = (max(addresses) - min(addresses)) // 8 + 1
            cells = []

            for k in keys:
                entries, shared = directory(moved, k)
                cells.append(f"{entries}{' SHARED' if shared else ''}")

            out.append(
                f"| {config} | {model} | {modules} | {len(moved)} "
                f"| {human(8 * flat)} "
                f"| {human(8 * two_step_slots(moved, 8))} "
                f"| {human(8 * two_step_slots(moved, 16))} |"
                + "".join(f" {cell} |" for cell in cells))

    text = "\n".join(out) + "\n"

    with open(args.markdown, "w") as f:
        f.write(text)

    print(text)
    print(f"-> {args.markdown}", file=sys.stderr)


if __name__ == "__main__":
    main()
