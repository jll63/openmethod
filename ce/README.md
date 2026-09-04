# Boost.OpenMethod on Compiler Explorer

Compiler Explorer can include a header from a URL, but only one file at a time:
it does not resolve the includes inside the file it fetches. `dev/flatten.py`
thus rewrites each public header into a self-sufficient one, and CI publishes
them at the root of <https://jll63.github.io/openmethod>, next to the
documentation.

An example on Compiler Explorer therefore includes exactly what it would
include locally, one line per header, in the same order:

```cpp
#include <https://jll63.github.io/openmethod/boost/openmethod.hpp>
#include <https://jll63.github.io/openmethod/boost/openmethod/initialize.hpp>
```

Every path under `include/boost/` is available under that URL - the interops,
the policies, `inplace_vptr.hpp`. Two things to know:

* `boost/openmethod.hpp` comes first. It is the only self-contained file; the
  others check that it has been included and stop with an `#error` otherwise.
* Select a Boost version in the *Libraries* dropdown. The flattened headers
  still include Boost.Mp11, Boost.DynamicBitset and the rest from Boost itself.

`-std=c++17 -O3 -DNDEBUG` is a good set of options to look at the generated
code.

## Reflection

`reflection.cpp` registers the classes by C++26 reflection (P2996). It includes
the same two headers as the other examples, but needs a compiler that
implements P2996 - *x86-64 gcc (trunk)* on Compiler Explorer - and
`-std=c++26 -freflection`.

`Bulldog` is the point of the example. No overrider mentions it, and no
`BOOST_OPENMETHOD_CLASSES` lists it; the scan started by
`BOOST_OPENMETHOD_REGISTER_CLASSES()` finds it deriving from `Dog`, registers
it, and `poke` dispatches it to the overrider for `Dog`.

## The sources

The sources in this directory are the examples published on Compiler Explorer.
They are built and run as part of the test suite, and again against the
flattened headers by `dev/check-flat.sh`, so a broken flattening is caught
before it reaches the site.

To generate the headers locally:

```bash
python3 dev/flatten.py               # writes flat/boost/...
BOOST_SRC_DIR=/path/to/boost dev/check-flat.sh
```
