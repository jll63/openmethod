// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: a group holds one kind of reflection

#include <boost/openmethod.hpp>

#if !BOOST_OPENMETHOD_HAS_REFLECTION

// Without reflection there is nothing to check; produce the expected
// diagnostic so the test passes under any configuration.
#error a group holds one kind of reflection

#else

namespace app {

struct my_registry : boost::openmethod::default_registry::with<> {};

struct Animal {
    virtual ~Animal() = default;
};

// A class and a registry cannot share a group.
BOOST_OPENMETHOD_REGISTER(
    boost::openmethod::register_classes<{^^app::Animal, ^^app::my_registry}>);

} // namespace app

#endif

int main() {
    return 0;
}
