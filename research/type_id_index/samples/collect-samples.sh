#!/bin/bash
# Collect runtime type_info addresses for every configuration produced by
# make-samples.sh (boostorg/openmethod#57).
#
# collect-type-infos loads each combination of a configuration's libraries in a
# forked process and reports the address dlsym resolves for every _ZTI symbol -
# i.e. exactly the value &typeid(X) yields. It writes 00000000.json... into the
# current directory, so each replicate gets its own.
#
# ASLR moves the modules on every run, so the *inter-module gaps* are a random
# variable; the replicates give its distribution. The intra-module layout is
# fixed by the ELF file and does not need replicating.
#
# Usage: ./collect-samples.sh [samples-root] [replicates-for-si-heavy]

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="${1:-$here/samples}"
main_replicates="${2:-20}"
collector="$here/build/collect-type-infos"

for dir in "$root"/*/; do
    name="$(basename "$dir")"
    replicates=3

    if [ "$name" = "si-heavy" ]; then
        replicates="$main_replicates"
    fi

    for r in $(seq 0 $((replicates - 1))); do
        out="$dir/runs/$r"
        mkdir -p "$out"
        (cd "$out" && "$collector" -i "$dir/libraries.json" -n 4 > /dev/null)
    done

    echo "$name: $replicates replicates"
done
