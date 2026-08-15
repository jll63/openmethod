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
using erased_ref = te::any<Concept, te::_self&>;

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD_REGISTER(use_type_erasure_types<erased_ref, Dog, int>);

// An any reference is a cheap handle; it is passed by value.
BOOST_OPENMETHOD(poke, (virtual_<erased_ref>), std::string);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog& dog), std::string) {
    dog.name += "!";
    return dog.name;
}

BOOST_OPENMETHOD_OVERRIDE(poke, (int& value), std::string) {
    ++value;
    return "poked";
}

#include <boost/openmethod/initialize.hpp>

int main() {
    initialize();

    Dog snoopy{"Snoopy"};
    int count = 41;

    // mutations reach the referents
    std::cout << poke(erased_ref(snoopy)) << "\n"; // Snoopy!
    std::cout << snoopy.name << "\n";              // Snoopy!

    std::cout << poke(erased_ref(count)) << "\n"; // poked
    std::cout << count << "\n";                   // 42
}
// end::content[]
