// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// This library owns and exports the registry state; main.cpp imports it. That
// is the natural direction for implicit linking: the executable already links
// this library, so on Windows the import-library dependency runs the usual way
// (see the "Static Linking" section of shared_libraries.adoc). LIB_SOURCE
// selects BOOST_OPENMETHOD_EXPORT_REGISTRY in registry.hpp.
#define LIB_SOURCE

#include "lib.hpp"

using namespace boost::openmethod;

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat);

BOOST_OPENMETHOD_OVERRIDE(speak, (virtual_ptr<Animal>), const char*) {
    return "?";
}

BOOST_OPENMETHOD_OVERRIDE(speak, (virtual_ptr<Dog>), const char*) {
    return "woof";
}

auto lib_registry_state_id() -> const void* {
    return custom_registry::id();
}

auto lib_speak(virtual_ptr<Animal> animal) -> const char* {
    return speak(animal);
}

auto lib_make_dog() -> unique_virtual_ptr<Animal> {
    return make_unique_virtual<Dog>();
}
