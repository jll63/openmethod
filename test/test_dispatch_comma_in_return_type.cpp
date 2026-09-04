// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <utility>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_classes.hpp"

#define BOOST_TEST_MODULE dispatch_comma_in_return_type
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

struct Test {
    virtual ~Test() {};
};

BOOST_OPENMETHOD_TEST_CLASSES(Test);

BOOST_OPENMETHOD(foo, (virtual_<Test&>), std::pair<int, int>);

BOOST_OPENMETHOD_OVERRIDE(foo, (Test&), std::pair<int, int>) {
    return {1, 2};
}

BOOST_AUTO_TEST_CASE(comma_in_return_type) {
    initialize();

    Test test;

    BOOST_CHECK(foo(test) == std::pair(1, 2));
}

// Registers the classes above by reflection, when the compiler supports it.
// Last in the file: the standard wants the registrar after what it selects.
BOOST_OPENMETHOD_REGISTER_CLASSES();
