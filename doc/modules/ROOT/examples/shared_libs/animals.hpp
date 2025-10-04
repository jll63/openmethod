// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ANIMALS_DEFINED
#define ANIMALS_DEFINED

#ifdef _WINDLL
#define DECLSPEC __declspec(dllimport)
#else
#define DECLSPEC __declspec(dllexport)
#endif

// clang-format off


// tag::content[]
// animals.hpp
#include <string>
#include <boost/openmethod.hpp>

struct Animal { virtual ~Animal() {} };
struct Herbivore : Animal {};
struct Carnivore : Animal {};

struct BOOST_OPENMETHOD_ID(meet);

namespace boost::openmethod {
template<>
struct declspec<
    method<
        BOOST_OPENMETHOD_ID(meet),
        std::string(virtual_ptr<Animal>, virtual_ptr<Animal>),
        void>> :
#ifdef _WINDLL
    declspec_import
#else
    declspec_export
#endif
    {};
}

BOOST_OPENMETHOD(
    meet, (
        boost::openmethod::virtual_ptr<Animal>,
        boost::openmethod::virtual_ptr<Animal>),
    std::string);

// end::content[]

#endif
