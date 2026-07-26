// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// extensions.cpp

// This shared library owns the registry state and the executable imports it.
// That keeps the link dependency in the usual direction: the executable links
// against the library, so on Windows it links the library's import library,
// never the other way around.
#define OWNS_REGISTRY_STATE

#include "animals.hpp"

using namespace boost::openmethod;

// The definition of the shared state. An explicit instantiation definition may
// appear only once in the program, so exactly one .cpp of the owning module
// emits it; the others get the `extern` declaration from animals.hpp.
BOOST_OPENMETHOD_EXPORT_REGISTRY(boost::openmethod::default_registry);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Herbivore> a, virtual_ptr<Carnivore> b), std::string) {
    auto p = BOOST_OPENMETHOD_TYPE(
        meet, (virtual_ptr<Animal>, virtual_ptr<Animal>),
        std::string)::next<fn>;
    // end::content[]
    BOOST_ASSERT(p);
    BOOST_ASSERT(p(a, b) == "greet");
    // tag::content[]
    return "run";
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Carnivore>, virtual_ptr<Herbivore>), std::string) {
    return "hunt";
}

struct Tiger : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Tiger, Carnivore);

extern "C" {
BOOST_SYMBOL_EXPORT auto make_tiger() -> Animal* {
    // end::content[]
    BOOST_ASSERT(default_registry::static_vptr<Carnivore> != nullptr);
    // tag::content[]
    return new Tiger;
}
}
// end::content[]
