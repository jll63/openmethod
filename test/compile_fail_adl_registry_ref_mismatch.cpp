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

// The contradiction of compile_fail_adl_registry_declared_mismatch.cpp, with
// the `virtual_ptr` passed by reference. `virtual_ptr<Animal>` carries
// `zoo_registry` in every shape.
BOOST_OPENMETHOD(
    poke, (const virtual_ptr<Animal>&), std::string, other_registry);

int main() {
    // See compile_fail_adl_registry_declared_mismatch.cpp for why the call.
    Animal animal;
    virtual_ptr<Animal> vp(animal);
    poke(vp);

    return 0;
}
