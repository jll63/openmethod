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

The sources in this directory are the examples published on Compiler Explorer.
They are built and run as part of the test suite, and again against the
flattened headers by `dev/check-flat.sh`, so a broken flattening is caught
before it reaches the site.

To generate the headers locally:

```bash
python3 dev/flatten.py               # writes flat/boost/...
BOOST_SRC_DIR=/path/to/boost dev/check-flat.sh
```
