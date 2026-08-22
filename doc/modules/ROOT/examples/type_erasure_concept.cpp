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

struct Dog {
    std::string name;
};

struct Dispatchable
    : boost::mpl::vector<
          te::copy_constructible<>, te::relaxed,
          openmethod_vptr<Dispatchable>> {};

using erased = te::any<Dispatchable>;

BOOST_OPENMETHOD(name, (virtual_<const erased&>), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const erased& value), std::string) {
    return te::is_empty(value) ? "nothing" : "something else";
}

#include <boost/openmethod/initialize.hpp>

int main() {
    initialize();

    const erased spot(Dog{"Spot"});
    const erased answer(42);

    std::cout << name(spot) << "\n";   // Spot the dog
    std::cout << name(answer) << "\n"; // something else
}
// end::content[]
