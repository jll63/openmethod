// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: do not wrap a virtual_any in a virtual_ptr

#include <any>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>

using namespace boost::openmethod;

struct Dog {
    std::string name;
};

int main() {
    // final_virtual_ptr does not go through acquire_vptr: it takes
    // static_vptr<virtual_traits<...>::virtual_type>, which for a virtual_any
    // is the `any` root class - so it would silently return the root's
    // v-table instead of the contained value's. It instantiates the
    // virtual_ptr it returns, so the rejection catches this too.
    virtual_std_any spot = Dog{"Spot"};
    final_virtual_ptr(spot);
    return 0;
}
