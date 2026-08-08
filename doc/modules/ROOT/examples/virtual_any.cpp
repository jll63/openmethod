// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::content[]
#include <any>
#include <iostream>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>

using namespace boost::openmethod;

struct Dog {
    Dog(std::string name) : name(std::move(name)) {}
    std::string name;
};

struct Cat {
    Cat(std::string name) : name(std::move(name)) {}
    std::string name;
};

// `std::any` becomes the common base of the types it may contain.
BOOST_OPENMETHOD_REGISTER(use_std_any_types<Dog, Cat, int>);

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

#include <boost/openmethod/initialize.hpp>

int main() {
    initialize();

    // From an existing `any`: the v-table pointer is looked up from the type
    // of the value it contains.
    std::any snoopy_any = Dog("Snoopy");
    virtual_std_any snoopy = snoopy_any;

    // From a value: the type is known at compile time, so the v-table pointer
    // is read from a static variable, with no lookup.
    virtual_std_any felix = Cat("Felix");

    // Same, constructing the value in place.
    auto hector = make_std_any_virtual<Dog>("Hector");

    std::cout << poke(snoopy) << "\n"; // Snoopy barks
    std::cout << poke(felix) << "\n";  // Felix hisses
    std::cout << poke(hector) << "\n"; // Hector barks

    // `int` is registered, but has no overrider of its own: the catch-all
    // applies. The value converts to a temporary `virtual_std_any` at the
    // call site.
    std::cout << poke(42) << "\n";     // it does nothing
}
// end::content[]
