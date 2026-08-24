// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: deleted function

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

int main() {
    // The primary final_virtual_ptr would use the static v-table pointer
    // of the any class itself - the root -, not the bound value's. The
    // combination is deleted; use virtual_any instead.
    erased spot(Dog{"Spot"});
    final_virtual_ptr(spot);
    return 0;
}
