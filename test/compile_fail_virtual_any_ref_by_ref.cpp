// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <any>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>

using namespace boost::openmethod;

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD_REGISTER(use_std_any_types<Dog>);

// A virtual_any_ref method parameter is passed by value: it is a cheap,
// two-word handle; a reference would add an indirection for nothing.
BOOST_OPENMETHOD(name, (const virtual_any_ref<const std::any>&), std::string);

int main() {
    std::any dog(Dog{"Snoopy"});
    return name(virtual_any_ref<const std::any>(dog)).size();
}
