// Copyright (c) 2018-2027 Jean-Louis Leroy
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
BOOST_OPENMETHOD_EXPORT_REGISTRY(boost::openmethod::default_registry);
#else
BOOST_OPENMETHOD_IMPORT_REGISTRY(boost::openmethod::default_registry);
#endif

struct Animal { virtual ~Animal() {} };
struct Herbivore : Animal {};
struct Carnivore : Animal {};

struct Cow : Herbivore {};
struct Wolf : Carnivore {};

BOOST_OPENMETHOD(
    meet, (
        boost::openmethod::virtual_ptr<Animal>,
        boost::openmethod::virtual_ptr<Animal>),
    std::string);
// end::content[]

#endif
