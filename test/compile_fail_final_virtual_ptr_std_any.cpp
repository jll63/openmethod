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

int main() {
    // The primary final_virtual_ptr would use static_vptr<std::any> - the
    // v-table of the `any` root class, not of the contained value. The
    // combination is deleted; use virtual_any instead.
    std::any spot(Dog{"Spot"});
    final_virtual_ptr(spot);
    return 0;
}
