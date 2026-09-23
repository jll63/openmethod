// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: deferred static rtti

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

// Standard RTTI, with the type ids resolved at initialize() time.
struct deferred_std_rtti : policies::deferred_static_rtti {
    template<class Registry>
    struct fn : policies::std_rtti::fn<Registry> {};
};

struct deferred_registry : default_registry::with<deferred_std_rtti> {};

struct Animal {
    virtual ~Animal() = default;
};

// The parameter's type ids are resolved by its registry, which defers, while
// the method's registry does not: the ids would be read before they exist.
BOOST_OPENMETHOD(
    poke, (virtual_<Animal&, deferred_registry>), void, default_registry);

int main() {
    // The check is in `method`'s class body. gcc and clang instantiate it
    // through the registrar, but MSVC does not: calling the method forces the
    // point on every compiler.
    Animal animal;
    poke(animal);

    return 0;
}
