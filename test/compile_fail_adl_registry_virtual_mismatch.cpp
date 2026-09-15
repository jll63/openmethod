// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: registry mismatch

#include <string>

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct zoo_registry : default_registry {};
struct other_registry : default_registry {};

struct Animal {
    virtual ~Animal() = default;
    friend auto boost_openmethod_registry(Animal*) -> zoo_registry;
};

// Same contradiction as compile_fail_adl_registry_declared_mismatch.cpp, in
// the `virtual_<>` shape: the class says `zoo_registry`, the method says
// otherwise.
BOOST_OPENMETHOD(poke, (virtual_<const Animal&>), std::string, other_registry);

int main() {
    // See compile_fail_adl_registry_declared_mismatch.cpp for why the call.
    Animal animal;
    poke(animal);

    return 0;
}
