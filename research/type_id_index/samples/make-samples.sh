#!/bin/bash
# Generate the sample matrix for the type_id -> index study
# (boostorg/openmethod#57).
#
# Two axes:
#   - inheritance shape, which sets the mix of type_info record sizes
#     (16 / 24 / 40 / 56 / 72 bytes) and hence the GCD of the addresses;
#   - translation-unit size, which sets how much the records are diluted by
#     the vtables emitted between them, and hence the density.
#
# Neither axis is tuned to a desired answer: the shape configurations bracket
# the plausible range, and the TU-size sweep is reported as a range.
#
# Usage: ./make-samples.sh [output-root]   (default: samples)

set -euo pipefail

root="${1:-samples}"
here="$(cd "$(dirname "$0")" && pwd)"
NL=4
NC=1000
ND=64

gen() {
    local name="$1"; shift
    echo "########## $name"
    python3 "$here/make-libs.py" "$NL" "$NC" "$ND" \
        -o "$root/$name" --print-shape -B "$@" | grep -Ev '^Generated lib'
}

# Shape axis, at the default 25 classes per translation unit.
gen flat       --frac-root 1.0 --frac-single 0 --frac-virtual 0 --frac-multi 0
gen tree       --frac-root 0.02 --frac-single 0.98 --frac-virtual 0 --frac-multi 0
gen si-heavy
gen mi-heavy   --frac-root 0.10 --frac-single 0.25 --frac-virtual 0.15 --frac-multi 0.50
gen virt-heavy --frac-root 0.10 --frac-single 0.25 --frac-virtual 0.50 --frac-multi 0.15

# Translation-unit-size axis, on the default shape.
gen si-heavy-tu10   --classes-per-tu 10
gen si-heavy-tu100  --classes-per-tu 100
gen si-heavy-tu1000 --classes-per-tu 1000   # single TU: the packed-records artefact

# One libraries.json per configuration, listing that configuration's libraries.
for dir in "$root"/*/; do
    name="$(basename "$dir")"
    python3 - "$dir" > "$dir/libraries.json" <<'EOF'
import glob, json, os, sys
libs = sorted(glob.glob(os.path.join(sys.argv[1], "build/libs/*.so")))
print(json.dumps([{"path": os.path.abspath(p), "typeinfo_count": 0}
                  for p in libs], indent=2))
EOF
    echo "wrote $dir/libraries.json"
done
