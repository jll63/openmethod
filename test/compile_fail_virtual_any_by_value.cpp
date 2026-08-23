// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: virtual_any must be passed by reference

#include <any>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>

using namespace boost::openmethod;

struct Dog {
    std::string name;
};

// A virtual_any method parameter must be a reference: passing it by value
// would copy the `any` - and its payload - on every call.
BOOST_OPENMETHOD(name, (virtual_std_any), std::string);

int main() {
    virtual_std_any dog = Dog{"Snoopy"};
    return name(dog).size();
}
