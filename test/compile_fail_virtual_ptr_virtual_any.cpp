// Copyright (c) 2018-2027 Jean-Louis Leroy
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

int main() {
    // A virtual_any already carries the v-table pointer for the value it
    // contains. Wrapping it would yield a wide pointer whose v-table is the
    // contained value's, but whose static type is the virtual_any - which is
    // deliberately not a registered class.
    virtual_std_any spot = Dog{"Spot"};
    virtual_ptr<virtual_std_any> p = spot;
    return 0;
}
