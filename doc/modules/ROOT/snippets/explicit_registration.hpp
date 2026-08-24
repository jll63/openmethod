// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Makes the default registry require explicit class registration, so that the
// error snippets keep reporting the error they illustrate when the compiler
// supports C++26 reflection. Include *before* <boost/openmethod.hpp>. Like
// error_harness.hpp, never part of a tagged region: the pages show the mistake
// and the operation that reports it, and nothing else.
//
// The errors themselves do not go away in C++26 -- a class the library cannot
// reach from a method signature, an overrider, or a virtual_ptr still has to be
// registered by hand -- but these particular examples are all within its reach.

#ifndef BOOST_OPENMETHOD_SNIPPETS_EXPLICIT_REGISTRATION_HPP
#define BOOST_OPENMETHOD_SNIPPETS_EXPLICIT_REGISTRATION_HPP

#include <boost/openmethod/default_registry.hpp>

struct snippet_registry :
    boost::openmethod::default_registry::with<
        boost::openmethod::policies::explicit_class_registration> {};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY snippet_registry

#endif
