// Copyright (c) 2018-2027 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_IMPLICIT_SHARED_LIBRARIES_CUSTOM_LIB_HPP
#define BOOST_OPENMETHOD_TEST_IMPLICIT_SHARED_LIBRARIES_CUSTOM_LIB_HPP

#include <boost/config.hpp>

// Defines custom_registry, sets it as the default registry, and exports or
// imports its state depending on LIB_SOURCE. Must come before any other
// openmethod header.
#include "registry.hpp"

#include <boost/openmethod/interop/std_unique_ptr.hpp>

// BOOST_SYMBOL_VISIBLE gives these classes' RTTI default visibility even in a
// hidden-visibility build, so their type_id unifies across the executable and
// the shared library. Same rationale as test/dynamic_loading/classes.hpp.
struct BOOST_SYMBOL_VISIBLE Animal {
    virtual ~Animal() = default;
};

struct BOOST_SYMBOL_VISIBLE Dog : Animal {};

struct BOOST_SYMBOL_VISIBLE Cat : Animal {};

// Overridden in lib2.cpp, the library's second translation unit.
struct BOOST_SYMBOL_VISIBLE Cow : Animal {};

// Methods are consolidated across modules at initialize() time, so they need no
// DLL decoration of their own. custom_registry is the default registry here
// (registry.hpp defines BOOST_OPENMETHOD_DEFAULT_REGISTRY), so it does not have
// to be named explicitly.
BOOST_OPENMETHOD(speak, (boost::openmethod::virtual_ptr<Animal>), const char*);

// lib.cpp defines LIB_SOURCE before including this header, so the library
// exports the entry points below and the executable imports them. Unlike
// test/dynamic_loading, nothing is looked up by name at run time - the
// executable links the library implicitly - so ordinary C++ linkage is enough,
// and the factory below can return a non-POD type directly.
#if defined(LIB_SOURCE)
#define LIB_API BOOST_SYMBOL_EXPORT
#else
#define LIB_API BOOST_SYMBOL_IMPORT
#endif

// The address identifying the registry's shared state, as seen from inside the
// library (see registry::id()). It must be identical to the address the
// executable sees.
LIB_API auto lib_registry_state_id() -> const void*;

// Dispatch performed inside the library, on an object created in either module.
LIB_API auto lib_speak(boost::openmethod::virtual_ptr<Animal> animal) -> const
    char*;

LIB_API auto lib_make_dog() -> boost::openmethod::unique_virtual_ptr<Animal>;

// Defined in lib2.cpp, a second translation unit of this library.
LIB_API auto lib_make_cow() -> boost::openmethod::unique_virtual_ptr<Animal>;

#endif
