// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ANIMALS_DEFINED
#define ANIMALS_DEFINED

// clang-format off

// tag::content[]
// animals.hpp

#include <string>
#include <boost/openmethod.hpp>

#ifdef OWNS_REGISTRY_STATE
extern template struct BOOST_SYMBOL_EXPORT
    boost::openmethod::registry_state<
        boost::openmethod::default_registry::registry_type>;
#else
extern template struct BOOST_SYMBOL_IMPORT
    boost::openmethod::registry_state<
        boost::openmethod::default_registry::registry_type>;
#endif

struct Animal { virtual ~Animal() {} };
struct Herbivore : Animal {};
struct Carnivore : Animal {};

struct Cow : Herbivore {};
struct Wolf : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Animal, Herbivore, Cow, Carnivore, Wolf);

BOOST_OPENMETHOD(
    meet, (
        boost::openmethod::virtual_ptr<Animal>,
        boost::openmethod::virtual_ptr<Animal>),
    std::string);
// end::content[]

#endif
