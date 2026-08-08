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
    std::string name;
};

// `std::any` becomes the common base of the types it may contain.
BOOST_OPENMETHOD_REGISTER(use_std_any_types<Dog, std::string, int, float>);

BOOST_OPENMETHOD(name, (virtual_<const std::any&>), std::string);

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

// ...or the `any` itself, which makes it a catch-all.
BOOST_OPENMETHOD_OVERRIDE(name, (const std::any&), std::string) {
    return "something else";
}

#include <boost/openmethod/initialize.hpp>

int main() {
    initialize();

    std::any spot = Dog{"Spot"};
    std::any felix = std::string("Felix the cat");
    std::any answer = 42;
    std::any pi = 3.14f;

    std::cout << name(spot) << "\n";   // Spot the dog
    std::cout << name(felix) << "\n";  // Felix the cat
    std::cout << name(answer) << "\n"; // 42 the integer

    // `float` is registered, but has no overrider of its own, so the
    // catch-all applies.
    std::cout << name(pi) << "\n";     // something else
}
// end::content[]
