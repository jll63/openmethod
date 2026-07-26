// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// This library owns and exports the registry state; main.cpp imports it. That
// is the natural direction for implicit linking: the executable already links
// this library, so on Windows the import-library dependency runs the usual way
// (see the "Static Linking" section of shared_libraries.adoc).
//
// OWNS_REGISTRY_STATE suppresses the import declaration in registry.hpp; it is
// defined by every translation unit of this library (see also lib2.cpp).
#define OWNS_REGISTRY_STATE

// Only the registry's definition, so that the export below is the first thing
// in this translation unit that mentions the state.
#include "registry.hpp"

// The export is an explicit instantiation *definition*, so it must appear
// exactly once in the program: in a .cpp, never in a header. This is that one
// place.
//
// It must also precede anything that uses the registry. A use would implicitly
// instantiate the state first, and under -fvisibility=hidden that instantiation
// is a module-local symbol; the BOOST_SYMBOL_EXPORT on a later explicit
// instantiation definition then arrives too late to promote it, leaving the
// library with a private copy and clients with an unresolved reference. Hence
// the split include: registry.hpp above, lib.hpp (which declares the method)
// below.
BOOST_OPENMETHOD_EXPORT_REGISTRY(custom_registry);

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
