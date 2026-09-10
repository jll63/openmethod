# Sample generator and address analysis

The other half of the harness in `..`: this generates the shared libraries whose
`type_info` records the prototypes are measured against, collects their runtime
addresses, and computes the table-size analyses that
[`../../../TYPE_ID_INDEX.md`](../../../TYPE_ID_INDEX.md) reports. Like the
prototypes it is **not part of Boost.OpenMethod's API**, has no Jamfile, and is
never built by CI.

It is a standalone CMake project rather than a subdirectory of the openmethod
build: the two collector programs need compiled Boost libraries (Filesystem,
System, ProgramOptions, JSON, DLL, via the `vcpkg.json` manifest) where the
library itself needs none. `BOOST_OPENMETHOD_BUILD_RESEARCH` does not reach it.
Configure it on its own:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

| file | what it does |
|---|---|
| `make-libs.py` | generates `NL` shared libraries of `NC` classes each, with a controlled mix of the four inheritance shapes (`--frac-root/-single/-virtual/-multi`) and a chosen translation-unit size (`--classes-per-tu`) |
| `make-samples.sh` | the sample matrix: five shapes at the default TU size, plus a TU-size sweep on `si-heavy`. §3 explains why those two axes are the ones that matter |
| `src/collect-libraries.cpp` | scans the machine for shared libraries containing `type_info` records, to JSON |
| `src/collect-type-infos.cpp` | `dlopen`s a combination of libraries in a forked process and reports the address and demangled name of every `type_info` it can reach - i.e. what `&typeid(X)` yields at run time |
| `collect-samples.sh` | runs the collector over each generated configuration |
| `analyze-type-ids.py` | record sizes, spans, densities and flat-table sizes: §§2, 4, 5. `--export-type-ids` writes the address lists in `../data/` |
| `analyze-bits.py` | the minimal-cover bit selection of §10, to `bits.md` |
| `analyze-two-step.py` | the per-module directory of §12, to `two-step.md` |

## Reproducing

```bash
./make-samples.sh && ./collect-samples.sh
python3 analyze-type-ids.py --export-type-ids ../data   # -> type-ids.csv, type-ids.md
python3 analyze-bits.py                                 # -> bits.md
python3 analyze-two-step.py                             # -> two-step.md
```

**A fresh collection will not reproduce the note's figures**, and that is
inherent rather than a defect: the addresses come from `dlopen` under ASLR, and
any quantity that depends on placement - every span, every table size, every
`fast_perfect_hash` search time - moves with them. The address sets the note was
actually computed from are committed in `../data/`; point the benchmarks at
those to check a table. Use this directory to generate *new* samples, to vary an
axis, or to check that a conclusion survives a different placement.

Generated output (`samples/`, `generated_libs/`, `build/`, the `.md` and `.csv`
analyses) is gitignored.
