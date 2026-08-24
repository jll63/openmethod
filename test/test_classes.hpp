// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_CLASSES_HPP
#define BOOST_OPENMETHOD_TEST_CLASSES_HPP

#include <boost/openmethod/detail/reflection.hpp>

//! Register classes, unless the library can find them by reflection.
//!
//! Expands to @ref BOOST_OPENMETHOD_CLASSES in C++17, and to nothing when the
//! compiler supports C++26 reflection. Tests that are not *about* class
//! registration use this instead of `BOOST_OPENMETHOD_CLASSES`, so that a
//! C++26 run exercises reflection-based registration over the whole suite: the
//! classes go unregistered, and every test still has to pass.
//!
//! Tests that check what happens when a class is *not* registered keep
//! `BOOST_OPENMETHOD_CLASSES`, and put
//! `boost::openmethod::policies::explicit_class_registration` in their
//! registry so that the library leaves the registration to them.

#if BOOST_OPENMETHOD_HAS_REFLECTION
#define BOOST_OPENMETHOD_TEST_CLASSES(...)
#else
#define BOOST_OPENMETHOD_TEST_CLASSES(...) BOOST_OPENMETHOD_CLASSES(__VA_ARGS__)
#endif

#endif
