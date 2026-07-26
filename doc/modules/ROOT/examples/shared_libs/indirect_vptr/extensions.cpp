// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// extensions.cpp

// This library is compiled with
// -DBOOST_OPENMETHOD_DEFAULT_REGISTRY=indirect_registry, so the registry in use
// is indirect_registry, which has its own state.

#include "animals.hpp"

using namespace boost::openmethod;

// The main program owns that state; this library does not define
// OWNS_REGISTRY_STATE, so animals.hpp imported it.

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
// end::content[]
