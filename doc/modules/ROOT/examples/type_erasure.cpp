// Copyright (c) 2018-2027 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::content[]
#include <iostream>
#include <string>

#include <boost/mpl/vector.hpp>
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/is_empty.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>

namespace te = boost::type_erasure;
using namespace boost::openmethod;

// `relaxed` implies `typeid_<>`, which dispatch relies on.
using Concept = boost::mpl::vector<te::copy_constructible<>, te::relaxed>;
using erased = te::any<Concept>;

struct Dog {
    std::string name;
};

// The owning `any`, `any<Concept>`, is the common base of the types the
// `any` may bind. An overrider registers the type it names as a class
// derived from it.
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

BOOST_OPENMETHOD(weigh, (virtual_<const erased&>), int);

BOOST_OPENMETHOD_OVERRIDE(weigh, (const int& value), int) {
    return value;
}

#include <boost/openmethod/initialize.hpp>

int main() {
    initialize();

    const erased spot(Dog{"Spot"});
    const erased felix(std::string("Felix the cat"));
    const erased answer(42);

    std::cout << name(spot) << "\n";  // Spot the dog
    std::cout << name(felix) << "\n"; // Felix the cat

    // `int` is registered - `weigh`'s overrider names it - but has no
    // `name` overrider of its own, so the catch-all applies.
    std::cout << weigh(answer) << "\n"; // 42
    std::cout << name(answer) << "\n";  // something else
}
// end::content[]
