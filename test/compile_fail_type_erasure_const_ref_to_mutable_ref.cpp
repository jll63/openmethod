// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: no matching

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

BOOST_OPENMETHOD(name, (virtual_<const erased&>), std::string);

// The `any` is const and owns its value, so the overrider cannot take a
// mutable reference to it; the `cast` overload is removed from the
// overload set.
BOOST_OPENMETHOD_OVERRIDE(name, (Dog & dog), std::string) {
    return dog.name;
}

int main() {
    return 0;
}
