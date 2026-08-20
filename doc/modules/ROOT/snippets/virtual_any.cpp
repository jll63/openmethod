// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <any>
#include <string>

#include <boost/any.hpp>
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/boost_any.hpp>
#include <boost/openmethod/interop/std_any.hpp>
#include <boost/openmethod/interop/virtual_any_ptr.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include "capture.hpp"

using namespace boost::openmethod;

namespace std_any {

// tag::classes[]
struct Dog {
    std::string name;
};

// `std::any` becomes the common base of the types it may contain.
BOOST_OPENMETHOD_REGISTER(use_std_any_types<Dog, std::string, int, float>);
// end::classes[]

// tag::method[]
BOOST_OPENMETHOD(name, (const virtual_std_any&), std::string);

// An overrider takes the contained value...
BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const std::string& name), std::string) {
    return name;
}

BOOST_OPENMETHOD_OVERRIDE(name, (const int& value), std::string) {
    return std::to_string(value) + " the integer";
}

// ...or the `virtual_any` itself, which makes it a catch-all.
BOOST_OPENMETHOD_OVERRIDE(name, (const virtual_std_any& value), std::string) {
    return value.get().has_value() ? "something else" : "nothing";
}
// end::method[]

} // namespace std_any

namespace boost_any {

// tag::boost_classes[]
struct Dog {
    std::string name;
};

// `boost::any` is a root class of its own, distinct from the one used for
// `std::any`, so both may be used in the same program and registry.
BOOST_OPENMETHOD_REGISTER(use_boost_any_types<Dog, std::string>);

BOOST_OPENMETHOD(name, (const virtual_boost_any&), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const std::string& name), std::string) {
    return name;
}
// end::boost_classes[]

} // namespace boost_any

namespace any_ptr {

using std_any::Dog;

// tag::ptr[]
BOOST_OPENMETHOD(poke, (virtual_any_ptr<std::any>), std::string);

// A plain value does not convert to a virtual_any_ptr, so overriders
// that take the contained value are registered with the core API.
using poke_method =
    BOOST_OPENMETHOD_TYPE(poke, (virtual_any_ptr<std::any>), std::string);

auto poke_dog(Dog& dog) -> std::string {
    dog.name += "!";
    return dog.name;
}

BOOST_OPENMETHOD_REGISTER(poke_method::override<poke_dog>);
// end::ptr[]

} // namespace any_ptr

BOOST_AUTO_TEST_CASE(std_any_examples) {
    using namespace std_any;

    initialize();

    {
        capture_cout cout;

        // tag::dispatch[]
        virtual_std_any spot = Dog{"Spot"};

        std::cout << name(spot) << "\n"; // Spot the dog

        // `float` is registered, but has no overrider of its own, so the
        // catch-all applies. The value converts to a temporary
        // `virtual_std_any` at the call site.
        std::cout << name(3.14f) << "\n"; // something else
        // end::dispatch[]

        BOOST_TEST(cout.str() == "Spot the dog\nsomething else\n");
    }

    {
        capture_cout cout;

        // tag::from_any[]
        std::any spot_any = Dog{"Spot"};

        // the v-table pointer is looked up from the type of the value the
        // `any` contains
        virtual_std_any spot = spot_any;

        std::cout << name(spot) << "\n"; // Spot the dog
        // end::from_any[]

        BOOST_TEST(cout.str() == "Spot the dog\n");
    }

    {
        capture_cout cout;

        // tag::from_value[]
        // the type is known at compile time, so the v-table pointer is read
        // from a static variable - there is no lookup
        virtual_std_any answer = 42;

        std::cout << name(answer) << "\n"; // 42 the integer
        // end::from_value[]

        BOOST_TEST(cout.str() == "42 the integer\n");
    }

    {
        capture_cout cout;

        // tag::emplace[]
        virtual_std_any value;

        value.emplace<std::string>("Felix the cat");

        std::cout << name(value) << "\n"; // Felix the cat
        // end::emplace[]

        BOOST_TEST(cout.str() == "Felix the cat\n");
    }
}

BOOST_AUTO_TEST_CASE(virtual_any_ptr_examples) {
    using namespace any_ptr;

    initialize();

    {
        capture_cout cout;

        // tag::ptr_dispatch[]
        std::any spot_any = Dog{"Spot"};

        // one lookup; the handle points to the `any`. The `any` type is
        // deduced, along with its constness
        virtual_any_ptr spot = spot_any;

        std::cout << poke(spot) << "\n"; // Spot!
        std::cout << poke(spot) << "\n"; // Spot!! - no lookup on any call
        // end::ptr_dispatch[]

        BOOST_TEST(cout.str() == "Spot!\nSpot!!\n");
    }
}

BOOST_AUTO_TEST_CASE(boost_any_examples) {
    using namespace boost_any;

    initialize();

    {
        capture_cout cout;

        // tag::boost_dispatch[]
        virtual_boost_any felix = std::string("Felix the cat");

        std::cout << name(felix) << "\n"; // Felix the cat
        // end::boost_dispatch[]

        BOOST_TEST(cout.str() == "Felix the cat\n");
    }
}
