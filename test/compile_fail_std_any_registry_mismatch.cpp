// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: registry mismatch

#include <any>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>

using namespace boost::openmethod;

struct zoo_registry : default_registry {};

// An interop parameter carries a registry exactly as an ordinary one does -
// see compile_fail_virtual_registry_mismatch.cpp - and must agree with its
// method. It used to be accepted silently, while still deciding the registry
// of a method that named none (#123).
BOOST_OPENMETHOD(
    speak, (virtual_<const std::any&, zoo_registry>), std::string,
    default_registry);

int main() {
    // See compile_fail_adl_registry_declared_mismatch.cpp for why the call.
    std::any animal;
    speak(animal);

    return 0;
}
