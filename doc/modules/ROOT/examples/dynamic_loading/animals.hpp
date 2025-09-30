// dl.hpp
// Copyright (c) 2018-2021 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ANIMALS_DEFINED
#define ANIMALS_DEFINED

// tag::header[]
// dl.hpp

#include <string>

#include <boost/openmethod.hpp>

struct Animal {
    virtual ~Animal() {
    }
};

struct Herbivore : Animal {};
struct Carnivore : Animal {};
struct Cow : Herbivore {};
struct Wolf : Carnivore {};

BOOST_OPENMETHOD(
    meet,
    (boost::openmethod::virtual_ptr<Animal>,
     boost::openmethod::virtual_ptr<Animal>),
    std::string);

// end::header[]

#endif
