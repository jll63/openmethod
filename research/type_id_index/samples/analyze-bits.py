#!/usr/bin/env python3
"""
Bit-selection analysis for boostorg/openmethod#57: a `pext`-based index.

The flat (ti - base) / d table fails because the *span* of the type ids sets
the table size, and the span is huge across modules. But a set of addresses
that are clustered per module and far apart between modules has structure in
its bits: the low bits vary within a module, the high bits vary between
modules - and the high bits are perfectly correlated with one another, since
they all encode nothing but "which module". Only ceil(log2(modules)) of them
carry information.

So: find the *minimal cover* - the smallest set of bit positions such that
`x & mask` is still injective over the set - and use `pext(x, mask)` (BMI2
parallel bit extract, one instruction) to pack those bits into a dense index in
[0, 2^popcount(mask)). Injectivity of pext(x, mask) is exactly injectivity of
x & mask, so the search never needs pext itself.

Inspired by "Perfect Hashing in an Imperfect World", Joaquín M. López Muñoz.

Two searches are tried, both deterministic:
  greedy-drop   start from every varying bit (trivially injective), try to
                drop each bit, lowest entropy first; keep the drop if the set
                stays injective. Locally minimal.
  greedy-add    start empty, repeatedly add the bit that resolves the most
                remaining collisions until injective. Then run greedy-drop on
                the result.
The better of the two is reported.
"""

import argparse
import glob
import importlib.util
import math
import os
import random
import statistics
import sys

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "analyze_type_ids", os.path.join(here, "analyze-type-ids.py"))
ati = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ati)


def bit_entropy(ids, bit):
    ones = sum((x >> bit) & 1 for x in ids)
    p = ones / len(ids)

    if p in (0.0, 1.0):
        return 0.0

    return -(p * math.log2(p) + (1 - p) * math.log2(1 - p))


def injective(ids, mask):
    return len({x & mask for x in ids}) == len(ids)


def collisions(ids, mask):
    seen = {}

    for x in ids:
        key = x & mask
        seen[key] = seen.get(key, 0) + 1

    return sum(c - 1 for c in seen.values())


def varying_mask(ids):
    mask = 0

    for x in ids:
        mask |= x ^ ids[0]

    return mask


def bits_of(mask):
    return [b for b in range(64) if (mask >> b) & 1]


def greedy_drop(ids, mask, order):
    """Drop bits in `order` while the set stays injective."""
    for bit in order:
        if not (mask >> bit) & 1:
            continue

        candidate = mask & ~(1 << bit)

        if injective(ids, candidate):
            mask = candidate

    return mask


def greedy_add(ids, universe):
    """Add the bit that resolves the most collisions until injective."""
    mask = 0
    remaining = list(bits_of(universe))

    while not injective(ids, mask):
        best = min(remaining, key=lambda b: collisions(ids, mask | (1 << b)))
        mask |= 1 << best
        remaining.remove(best)

    return mask


def minimal_cover(ids):
    ids = sorted(set(ids))
    universe = varying_mask(ids)
    entropy = {b: bit_entropy(ids, b) for b in bits_of(universe)}

    # Lowest entropy first: those bits separate the fewest pairs.
    drop_order = sorted(entropy, key=lambda b: entropy[b])
    by_drop = greedy_drop(ids, universe, drop_order)

    by_add = greedy_add(ids, universe)
    by_add = greedy_drop(ids, by_add, drop_order)

    best = min(by_drop, by_add, key=lambda m: bin(m).count("1"))

    return best, universe, entropy


def describe(label, records, out, fph):
    ids = [address for address, _, _ in records]
    mask, universe, entropy = minimal_cover(ids)
    n = len(set(ids))
    k = bin(mask).count("1")
    lower = math.ceil(math.log2(n))
    table = 8 << k
    modules = len({m for _, _, m in records})

    row = {
        "label": label,
        "modules": modules,
        "n": n,
        "varying_bits": bin(universe).count("1"),
        "cover_bits": k,
        "lower_bound_bits": lower,
        "table": table,
        "fph_table": fph(n),
        "blowup": table / fph(n),
        "bytes_per_class": table / n,
        "mask": mask,
        "cover": bits_of(mask),
    }
    out.append(row)

    return row


def human(value):
    return ati.human(int(value))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples-root", default=os.path.join(here, "samples"))
    parser.add_argument("--placement", default="observed,adjacent,pie-split")
    parser.add_argument("--markdown", default="bits.md")
    args = parser.parse_args()

    def fph(n):
        return 8 * ati.fph_buckets(n)

    rows = []
    configs = sorted(
        d for d in os.listdir(args.samples_root)
        if os.path.isdir(os.path.join(args.samples_root, d)))

    for config in configs:
        # replicate 0 only: the intra-module layout is fixed by the ELF file,
        # and the placement models cover the inter-module part.
        for path in sorted(glob.glob(os.path.join(
                args.samples_root, config, "runs", "0", "*.json"))):
            records, libraries = ati.load_sample(path)

            if len(records) < 2:
                continue

            combo = os.path.splitext(os.path.basename(path))[0]
            modules = len({m for _, _, m in records})

            # Single-module combos need no placement model.
            models = ["observed"] if modules == 1 else args.placement.split(",")

            for model in models:
                placed = ati.place(records, model)
                describe(f"{config}/{combo}/{model}", placed, rows, fph)

    lines = ["### Minimal-cover bit selection (pext index)\n",
             "| sample | modules | N | varying bits | cover bits | lower bound "
             "| table | fph table | blowup | B/class |",
             "|---|---|---|---|---|---|---|---|---|---|"]

    for r in rows:
        lines.append(
            f"| {r['label']} | {r['modules']} | {r['n']} | {r['varying_bits']} "
            f"| **{r['cover_bits']}** | {r['lower_bound_bits']} "
            f"| {human(r['table'])} | {human(r['fph_table'])} "
            f"| {r['blowup']:.1f}x | {r['bytes_per_class']:.0f} |")

    with open(args.markdown, "w") as f:
        f.write("\n".join(lines) + "\n")

    print("\n".join(lines))


if __name__ == "__main__":
    main()
