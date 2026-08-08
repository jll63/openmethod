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

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include "capture.hpp"

using namespace boost::openmethod;

namespace std_any {

// tag::classes[]
struct Dog {
    Dog(std::string name) : name(std::move(name)) {
    }

    std::string name;
};

struct Cat {
    Cat(std::string name) : name(std::move(name)) {
    }

    std::string name;
};

// `std::any` becomes the common base of the types it may contain.
BOOST_OPENMETHOD_REGISTER(use_std_any_types<Dog, Cat, int>);
// end::classes[]

// tag::method[]
BOOST_OPENMETHOD(poke, (const virtual_std_any&), std::string);

// An overrider takes the contained value...
BOOST_OPENMETHOD_OVERRIDE(poke, (const Dog& dog), std::string) {
    return dog.name + " barks";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (const Cat& cat), std::string) {
    return cat.name + " hisses";
}

// ...or the `virtual_any` itself, which makes it a catch-all.
BOOST_OPENMETHOD_OVERRIDE(poke, (const virtual_std_any& value), std::string) {
    return value.get().has_value() ? "it does nothing" : "nothing happens";
}
// end::method[]

} // namespace std_any

namespace boost_any {

// tag::boost_classes[]
struct Dog {
    Dog(std::string name) : name(std::move(name)) {
    }

    std::string name;
};

// `boost::any` is a root class of its own, distinct from the one used for
// `std::any`, so both may be used in the same program and registry.
BOOST_OPENMETHOD_REGISTER(use_boost_any_types<Dog>);

BOOST_OPENMETHOD(poke, (const virtual_boost_any&), std::string);

BOOST_OPENMETHOD_OVERRIDE(poke, (const Dog& dog), std::string) {
    return dog.name + " barks";
}
// end::boost_classes[]

} // namespace boost_any

BOOST_AUTO_TEST_CASE(std_any_examples) {
    using namespace std_any;

    initialize();

    {
        capture_cout cout;

        // tag::dispatch[]
        virtual_std_any snoopy = Dog("Snoopy");

        std::cout << poke(snoopy) << "\n"; // Snoopy barks

        // `int` is registered, but has no overrider of its own, so the
        // catch-all applies. The value converts to a temporary
        // `virtual_std_any` at the call site.
        std::cout << poke(42) << "\n"; // it does nothing
        // end::dispatch[]

        BOOST_TEST(cout.str() == "Snoopy barks\nit does nothing\n");
    }

    {
        capture_cout cout;

        // tag::from_any[]
        std::any snoopy_any = Dog("Snoopy");

        // the v-table pointer is looked up from the type of the value the
        // `any` contains
        virtual_std_any snoopy = snoopy_any;

        std::cout << poke(snoopy) << "\n"; // Snoopy barks
        // end::from_any[]

        BOOST_TEST(cout.str() == "Snoopy barks\n");
    }

    {
        capture_cout cout;

        // tag::from_value[]
        // `Cat` is known at compile time, so the v-table pointer is read
        // from a static variable - there is no lookup
        virtual_std_any felix = Cat("Felix");

        std::cout << poke(felix) << "\n"; // Felix hisses
        // end::from_value[]

        BOOST_TEST(cout.str() == "Felix hisses\n");
    }

    {
        capture_cout cout;

        // tag::emplace[]
        virtual_std_any animal;

        animal.emplace<Cat>("Felix");

        std::cout << poke(animal) << "\n"; // Felix hisses
        // end::emplace[]

        BOOST_TEST(cout.str() == "Felix hisses\n");
    }

    {
        capture_cout cout;

        // tag::make_any_virtual[]
        auto felix = make_any_virtual<Cat, std::any>("Felix");

        std::cout << poke(felix) << "\n"; // Felix hisses
        // end::make_any_virtual[]

        BOOST_TEST(cout.str() == "Felix hisses\n");
    }

    {
        capture_cout cout;

        // tag::make_std_any_virtual[]
        auto snoopy = make_std_any_virtual<Dog>("Snoopy");

        std::cout << poke(snoopy) << "\n"; // Snoopy barks
        // end::make_std_any_virtual[]

        BOOST_TEST(cout.str() == "Snoopy barks\n");
    }
}

BOOST_AUTO_TEST_CASE(boost_any_examples) {
    using namespace boost_any;

    initialize();

    {
        capture_cout cout;

        // tag::boost_dispatch[]
        auto snoopy = make_boost_any_virtual<Dog>("Snoopy");

        std::cout << poke(snoopy) << "\n"; // Snoopy barks
        // end::boost_dispatch[]

        BOOST_TEST(cout.str() == "Snoopy barks\n");
    }
}
