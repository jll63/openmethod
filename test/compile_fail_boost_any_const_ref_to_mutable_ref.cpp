// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// The constrained `cast` is removed from the overload set, so the diagnostic is
// the compiler's own overload resolution failure, whose wording varies: "no
// matching function for call to" on clang and gcc, "no matching overloaded
// function found" on MSVC.
// expected-error: no matching

#include <string>
#include <boost/any.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_any.hpp>

using namespace boost::openmethod;

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD(name, (virtual_<const boost::any&>), std::string);

// The `any` is const, so boost::any_cast cannot produce a mutable reference to
// the value it contains. Without the constraint on `cast`, this would fail
// inside Boost.Any instead of at the trait.
BOOST_OPENMETHOD_OVERRIDE(name, (Dog & dog), std::string) {
    return dog.name;
}

int main() {
    return 0;
}
