// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include <boost/mpl/vector.hpp>
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/is_empty.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include "capture.hpp"

namespace te = boost::type_erasure;
using namespace boost::openmethod;

// tag::classes[]
// `relaxed` implies `typeid_<>`, which dispatch relies on.
using Concept = boost::mpl::vector<te::copy_constructible<>, te::relaxed>;
using erased = te::any<Concept>;

struct Dog {
    std::string name;
};

// The owning `any`, `any<Concept>`, becomes the common base of the types
// the `any` may bind.
BOOST_OPENMETHOD_REGISTER(
    use_type_erasure_types<erased, Dog, std::string, int>);
// end::classes[]

// tag::method[]
BOOST_OPENMETHOD(name, (virtual_<const erased&>), std::string);

// An overrider takes the bound value...
BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const std::string& name), std::string) {
    return name;
}

// ...or the `any` itself, which makes it a catch-all.
BOOST_OPENMETHOD_OVERRIDE(name, (const erased& value), std::string) {
    return te::is_empty(value) ? "nothing" : "something else";
}
// end::method[]

BOOST_AUTO_TEST_CASE(type_erasure_examples) {
    initialize();

    {
        capture_cout cout;

        // tag::dispatch[]
        const erased spot(Dog{"Spot"});
        const erased answer(42);

        std::cout << name(spot) << "\n"; // Spot the dog

        // `int` is registered, but has no overrider of its own, so the
        // catch-all applies.
        std::cout << name(answer) << "\n"; // something else
        // end::dispatch[]

        BOOST_TEST(cout.str() == "Spot the dog\nsomething else\n");
    }
}
