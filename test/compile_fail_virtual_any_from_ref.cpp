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
    std::any dog(Dog{"Snoopy"});
    virtual_any_ref<std::any> ref(dog);

    // A virtual_any cannot be copy-initialized from a virtual_any_ref. The
    // value constructor is constrained to reject every wide type, so storing
    // the handle inside the `any` would take two user-defined conversions -
    // virtual_any_ref to std::any, then std::any to virtual_any - which is
    // one more than an implicit conversion sequence allows. Were the handle
    // stored, its static_vptr would be null: a handle is not a registered
    // class.
    virtual_std_any copy = ref;

    return 0;
}
