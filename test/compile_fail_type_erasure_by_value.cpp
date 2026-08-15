// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>

#include <boost/type_erasure/builtin.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>

namespace te = boost::type_erasure;
using namespace boost::openmethod;

using Concept = boost::mpl::vector<te::copy_constructible<>, te::relaxed>;
using erased = te::any<Concept>;

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD_REGISTER(use_type_erasure_types<erased, Dog>);

// The owning any must be passed by reference: by value, it would copy
// the `any` - and its payload - on every call. (The any references,
// any<C, _self&> and any<C, const _self&>, may be passed by value.)
BOOST_OPENMETHOD(name, (virtual_<erased>), std::string);

int main() {
    // Call the method: declaring it is not enough to instantiate it on
    // every compiler, and the guard lives in the method's body.
    erased dog = Dog{"Snoopy"};
    return name(dog).size();
}
