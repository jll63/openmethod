// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <any>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>
#include <boost/openmethod/interop/virtual_any_ptr.hpp>

using namespace boost::openmethod;

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD_REGISTER(use_std_any_types<Dog>);

int main() {
    std::any dog(Dog{"Snoopy"});
    virtual_any_ptr<const std::any> ptr(dog);

    // `emplace` is the only way to replace the value contained in the
    // `any` through a handle, and a const handle does not have it: the
    // `any` is not modifiable through it at all.
    ptr.emplace<Dog>(Dog{"Hector"});

    return 0;
}
