// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: registry mismatch

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct other_registry : default_registry {};

struct Cat {
    virtual ~Cat() = default;
};

// A registry spelled on a parameter is not the class's affinity. Cat declares
// none, so the method lands in the default registry, and the parameter then
// contradicts it - as it did before affinities existed. The method has to name
// its registry: `BOOST_OPENMETHOD(poke, (virtual_ptr<Cat, other_registry>),
// void, other_registry)`.
BOOST_OPENMETHOD(poke, (virtual_ptr<Cat, other_registry>), void);

int main() {
    // See compile_fail_adl_registry_declared_mismatch.cpp for why the call.
    Cat felix;
    poke(felix);

    return 0;
}
