// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// extensions.cpp

#define OWNS_REGISTRY_STATE

#include "animals.hpp"

using namespace boost::openmethod;

BOOST_OPENMETHOD_EXPORT_REGISTRY(boost::openmethod::default_registry);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Herbivore> a, virtual_ptr<Carnivore> b), std::string) {
    auto base = next(a, b);
    return "do not " + base + ", run";
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Carnivore>, virtual_ptr<Herbivore>), std::string) {
    return "hunt";
}

struct Tiger : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Tiger, Carnivore);

extern "C" {
BOOST_SYMBOL_EXPORT auto make_tiger() -> Animal* {
    return new Tiger;
}
}
// end::content[]
