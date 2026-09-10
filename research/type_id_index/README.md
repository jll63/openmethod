# `type_id` -> index by subtraction and division

Research prototypes for [boostorg/openmethod#57](https://github.com/boostorg/openmethod/issues/57).
**Not part of Boost.OpenMethod's API**, not built by default, and deliberately
absent from the Jamfile so CI never sees it.

The findings are in [`../../TYPE_ID_INDEX.md`](../../TYPE_ID_INDEX.md). Short
version: prototypes **A** and **B**, the flat index #57 asked about, are
provably injective and generate the single fused load they promise, but buy
about a nanosecond for 5x-149x more memory - infinitely more once a program
`dlopen`s a module that registers classes. Three more schemes were tried
against them.

**C**, the minimal-cover `pext` index (§10), is immune to module placement, as
fast as the incumbent, and costs about twice its memory. **D**, a
hash-and-displace minimal perfect hash (§11), is the only one that costs *less*
memory than the incumbent - a flat 9.45 bytes per type id, whatever the
addresses are - and builds it 3x-5000x faster, but a second dependent load on
the dispatch path makes it 1-2 ns per call slower. **E** (§13) is D with the
final reduction replaced by a shift into a power-of-two table: a nanosecond
cheaper on GCC, no cheaper on clang, and it trades D's flat table size for a
sawtooth.

A per-module directory keyed by the high bits (§12) was analysed rather than
built - it removes the multi-module span entirely and still loses, because the
within-module dilution was the expensive half all along.

None of them is worth shipping as the general v-table lookup. The useful
by-product is a measurement of how expensive `fast_perfect_hash`'s search
actually is - it aborts outright on the 4000-class samples - and two candidates
to put on that failure path, of which prototype D is the stronger.

| file | what it is |
|---|---|
| `direct_type_id_index.hpp` | prototype A: a `type_hash` policy, `H(x) = (x - base) / Divisor` |
| `vptr_direct_vector.hpp` | prototype B: a `vptr` policy that folds the base into the table's address, for the one-instruction lookup |
| `pext_index.hpp` | prototype C: a `type_hash` policy indexing by `pext(x, mask)` over a minimal cover of the type ids' bits; needs BMI2 |
| `hd_index.hpp` | prototype D: a `type_hash` policy that is a minimal perfect hash, by hash and displace; one slot per type id |
| `two_level_index.hpp` | prototype E: prototype D with the reduction replaced by a shift, `(m2[h1(x)] * h) >> s2`, into a power-of-two table |
| `registry.hpp` | registry recipes, selected by `-DRESEARCH_POLICY=`; force-include it to re-run an existing test against a prototype |
| `check_prototype.cpp` | properties of the prototypes, and of the ABI claim they rest on |
| `msvc_direct_rtti.cpp` | MSVC x64 only, outside the CMake harness: can `&typeid(obj)` be read inline from the complete object locator, and what is it worth (§15) |
| `bench_dispatch.cpp` | dispatch through a plain reference - the only path that uses the map |
| `bench_initialize.cpp` | policy `initialize` against a stand-in `InitializeContext` |

Built and run on GCC 13.3, clang 22 and MSVC 19.51; see `TYPE_ID_INDEX.md`
§14 for the MSVC recipe and what it changes.

```bash
cmake -B build-research -DBOOST_SRC_DIR=/path/to/boost \
      -DBOOST_OPENMETHOD_BUILD_TESTS=ON -DBOOST_OPENMETHOD_BUILD_RESEARCH=ON \
      -DCMAKE_BUILD_TYPE=Release
capped -m 24G -- cmake --build build-research --target research-all -j4
ctest --test-dir build-research -R '^research-'
cmake --build build-research --target research-disasm
```

Build it under a memory cap: `bench_dispatch.cpp`'s compile cost is roughly
quadratic in the registered class count, and an earlier 512-class version
reached 23.5 GB resident per translation unit - enough, at `-j12`, to fire the
global OOM killer. It registers 64 classes now (0.7 GB, 5 s). See
`TYPE_ID_INDEX.md`, *Appendix: building the harness*.

The sample generator and the address analysis live in a separate repository,
`typeinfo-samples`.
