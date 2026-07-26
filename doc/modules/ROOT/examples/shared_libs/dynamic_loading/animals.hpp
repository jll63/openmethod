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

// The module that owns the registry state defines OWNS_REGISTRY_STATE before
// including this header, and emits the matching definition - plain
// BOOST_OPENMETHOD_EXPORT_REGISTRY - in exactly one of its .cpp files. Every
// other translation unit, in that module and in every other, gets a
// declaration here. Prefixing EXPORT with `extern` turns the definition into
// such a declaration.
//
// This module has a single translation unit, so the declaration below is not
// strictly required; it is shown because it *is* required as soon as the owning
// module has more than one. Omitting it there leaves those translation units
// instantiating the state implicitly, and under -fvisibility=hidden the merged
// symbol is demoted to module-local, so clients fail to link.
#ifdef OWNS_REGISTRY_STATE
extern BOOST_OPENMETHOD_EXPORT_REGISTRY(boost::openmethod::default_registry);
#else
BOOST_OPENMETHOD_IMPORT_REGISTRY(boost::openmethod::default_registry);
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
