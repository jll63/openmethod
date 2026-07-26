// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE implicit_shared_libraries_default_registry

#include <boost/test/unit_test.hpp>

#include "lib.hpp"

// The shared library owns and exports the registry state (see lib.cpp); this
// module imports it.
BOOST_OPENMETHOD_IMPORT_REGISTRY(boost::openmethod::default_registry);

#include <boost/openmethod/initialize.hpp>

#include <string>

using namespace boost::openmethod;

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat);

// Registered by the executable; the library knows nothing about it.
BOOST_OPENMETHOD_OVERRIDE(speak, (virtual_ptr<Cat>), const char*) {
    return "meow";
}

// The library is linked implicitly, so its static constructors have already run
// by the time we get here: a single initialize() sets up dispatch tables that
// include both modules' contributions.
BOOST_AUTO_TEST_CASE(shared_registry_state) {
    // The registry's state must be one symbol, shared by both modules, not a
    // private copy per module.
    BOOST_TEST(lib_registry_state_id() == default_registry::id());

    initialize();

    auto lib_dog = lib_make_dog();
    auto exe_dog = make_unique_virtual<Dog>();
    BOOST_TEST(lib_dog.vptr() == exe_dog.vptr());

    // The library's Dog overrider is visible to the executable...
    BOOST_TEST(std::string(speak(lib_dog)) == "woof");
    BOOST_TEST(std::string(lib_speak(exe_dog)) == "woof");

    // ...and the executable's Cat overrider is visible to the library. This is
    // the strong check: it passes only if both modules registered into the same
    // dispatch tables.
    auto cat = make_unique_virtual<Cat>();
    BOOST_TEST(std::string(lib_speak(cat)) == "meow");
    BOOST_TEST(std::string(speak(cat)) == "meow");

    // A plain Animal falls back to the library's base overrider.
    auto animal = make_unique_virtual<Animal>();
    BOOST_TEST(std::string(lib_speak(animal)) == "?");
}
