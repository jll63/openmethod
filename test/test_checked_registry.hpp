// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_CHECKED_REGISTRY_HPP
#define BOOST_OPENMETHOD_TEST_CHECKED_REGISTRY_HPP

// This header owns the whole registry recipe for the tests that expect a
// registration error to be thrown: the declaration, the
// BOOST_OPENMETHOD_DEFAULT_REGISTRY definition, the library include, and
// `test_registry` itself. Including it first - before anything that pulls in
// core.hpp - is all a test has to do, and there is no ordering left for a
// caller to get wrong.
//
// `runtime_checks` catches what initialize() cannot; `throw_error_handler`
// turns the diagnosis into an exception the test can catch.
//
// A test that is *about* a class the library must not find on its own - one
// that expects `missing_class` or `missing_base` - defines
// BOOST_OPENMETHOD_TEST_EXPLICIT_CLASS_REGISTRATION before including this
// header. The `explicit_class_registration` policy then leaves every
// registration to the test, exactly as in C++17, instead of letting reflection
// supply the class the test is withholding.
struct test_registry;
#define BOOST_OPENMETHOD_DEFAULT_REGISTRY test_registry

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>

#ifdef BOOST_OPENMETHOD_TEST_EXPLICIT_CLASS_REGISTRATION
struct test_registry :
    boost::openmethod::default_registry::with<
        boost::openmethod::policies::runtime_checks,
        boost::openmethod::policies::throw_error_handler,
        boost::openmethod::policies::explicit_class_registration> {};
#else
struct test_registry :
    boost::openmethod::default_registry::with<
        boost::openmethod::policies::runtime_checks,
        boost::openmethod::policies::throw_error_handler> {};
#endif

#endif
