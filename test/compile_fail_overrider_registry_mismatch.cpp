// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: registry mismatch

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct zoo_registry : default_registry {};
struct kennel_registry : default_registry {};

struct Animal {
    virtual ~Animal() = default;
    friend auto boost_openmethod_registry(Animal*) -> zoo_registry;
};

// Poodle declares an affinity of its own, so `virtual_ptr<Poodle>` is a
// `virtual_ptr` in `kennel_registry` while the method's parameter is one in
// `zoo_registry`, and the two do not convert. Without the guide that ignores
// registries, this reports only that no `poke` accepts these arguments.
struct Poodle : Animal {
    friend auto boost_openmethod_registry(Poodle*) -> kennel_registry;
};

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Poodle>), void) {
}

int main() {
    return 0;
}
