// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>
#include <boost/openmethod/initialize.hpp>

#include <memory>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

// tag::classes[]
struct Animal : inplace_vptr_base<Animal> {};

struct Cat : Animal, inplace_vptr_derived<Cat, Animal> {};

struct Dog : Animal, inplace_vptr_derived<Dog, Animal> {};

BOOST_OPENMETHOD(trick, (virtual_<Animal&> animal), std::string);

BOOST_OPENMETHOD_OVERRIDE(trick, (Cat&), std::string) {
    return "sulk";
}

BOOST_OPENMETHOD_OVERRIDE(trick, (Dog&), std::string) {
    return "spin";
}
// end::classes[]

BOOST_AUTO_TEST_CASE(inplace_vptr_examples) {
    // tag::dispatch[]
    initialize();

    std::unique_ptr<Animal> a = std::make_unique<Cat>();
    std::unique_ptr<Animal> b = std::make_unique<Dog>();

    BOOST_TEST(trick(*a) == "sulk");
    BOOST_TEST(trick(*b) == "spin");
    // end::dispatch[]
}
