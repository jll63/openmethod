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

// `std::any` is the common base of the types it may contain. An overrider
// registers the type it names as a class derived from it.
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

BOOST_OPENMETHOD(weigh, (virtual_<const std::any&>), float);

BOOST_OPENMETHOD_OVERRIDE(weigh, (const float& value), float) {
    return value;
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

    // `float` is registered - `weigh`'s overrider names it - but has no
    // `name` overrider of its own, so the catch-all applies.
    std::cout << weigh(pi) << "\n";    // 3.14
    std::cout << name(pi) << "\n";     // something else
}
// end::content[]
