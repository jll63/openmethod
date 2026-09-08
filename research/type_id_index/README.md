# `type_id` -> index by subtraction and division

Research prototypes for [boostorg/openmethod#57](https://github.com/boostorg/openmethod/issues/57).
**Not part of Boost.OpenMethod's API**, not built by default, and deliberately
absent from the Jamfile so CI never sees it.

The findings are in [`../../TYPE_ID_INDEX.md`](../../TYPE_ID_INDEX.md). Short
version: the mapping is provably injective and generates the single fused load
it promises, but it is not measurably faster and it costs 12x-500x more memory -
infinitely more once a program spans an executable and a shared library. The
useful by-product is a measurement of how expensive `fast_perfect_hash`'s search
actually is.

| file | what it is |
|---|---|
| `direct_type_id_index.hpp` | prototype A: a `type_hash` policy, `H(x) = (x - base) / Divisor` |
| `vptr_direct_vector.hpp` | prototype B: a `vptr` policy that folds the base into the table's address, for the one-instruction lookup |
| `registry.hpp` | registry recipes, selected by `-DRESEARCH_POLICY=`; force-include it to re-run an existing test against a prototype |
| `check_prototype.cpp` | properties of the prototypes, and of the ABI claim they rest on |
| `bench_dispatch.cpp` | dispatch through a plain reference - the only path that uses the map |
| `bench_initialize.cpp` | policy `initialize` against a stand-in `InitializeContext` |

```bash
cmake -B build-research -DBOOST_SRC_DIR=/path/to/boost \
      -DBOOST_OPENMETHOD_BUILD_TESTS=ON -DBOOST_OPENMETHOD_BUILD_RESEARCH=ON \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-research --target research-all -j12
ctest --test-dir build-research -R '^research-'
cmake --build build-research --target research-disasm
```

The sample generator and the address analysis live in a separate repository,
`typeinfo-samples`.
