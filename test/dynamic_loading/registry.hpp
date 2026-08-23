// Copyright (c) 2018-2027 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_DYNAMIC_LOADING_REGISTRY_HPP
#define BOOST_OPENMETHOD_TEST_DYNAMIC_LOADING_REGISTRY_HPP

// The registry under test. The build selects the variant by defining
// BOOST_OPENMETHOD_DEFAULT_REGISTRY on the command line (e.g. to
// ::boost::openmethod::indirect_registry). With no switch, core.hpp supplies
// its own default, default_registry. Either way the macro is defined and the
// registry complete once this header has been included - which is why the
// alias below comes after it, not before.
#include <boost/openmethod.hpp>

using test_registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY;

// The module that owns the state compiles with EXPORT_REGISTRY defined; every
// other module imports it. The visibility attribute goes on the declaration;
// the definition that follows carries none (repeating it is an error on GCC).
// Each module here has a single translation unit, so the definition can sit in
// this header; a multi-TU owner would keep only the declaration here and put
// the definition in one of its .cpp files.
//
// This is done on every platform, not just Windows: the modules are built with
// hidden visibility in the super-project, where an implicitly instantiated
// registry state would not be shared across the shared objects.
#if defined(EXPORT_REGISTRY)
BOOST_OPENMETHOD_EXPORT_REGISTRY(test_registry);
BOOST_OPENMETHOD_INSTANTIATE_REGISTRY(test_registry);
#else
BOOST_OPENMETHOD_IMPORT_REGISTRY(test_registry);
#endif

// The single address that identifies the registry's shared state. It must be
// identical across all modules that share the registry (see registry::id()).
// inline (not static): registry.cpp never calls it, and an unreferenced static
// function trips MSVC's C4505 under /WX.
inline auto registry_state_id() -> const void* {
    return test_registry::id();
}

#endif
