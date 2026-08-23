// Copyright (c) 2018-2026 Jean-Louis Leroy
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

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>

namespace te = boost::type_erasure;
using namespace boost::openmethod;

using Concept = boost::mpl::vector<te::copy_constructible<>, te::relaxed>;
using erased = te::any<Concept>;

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD(name, (const virtual_any<erased>&), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const std::string& name), std::string) {
    return name;
}

#include <boost/openmethod/initialize.hpp>

int main() {
    initialize();

    // from a value: the v-table pointer is set statically
    virtual_any<erased> rex = Dog{"Rex"};
    std::cout << (rex.vptr() == default_registry::static_vptr<Dog>) << "\n"; // 1

    // from an `any`: one lookup, here, and none in the calls below
    erased spot_any(Dog{"Spot"});
    virtual_any<erased> spot = spot_any;

    std::cout << name(rex) << "\n";  // Rex the dog
    std::cout << name(spot) << "\n"; // Spot the dog

    spot = std::string("Felix the cat");
    std::cout << name(spot) << "\n"; // Felix the cat
}
// end::content[]
