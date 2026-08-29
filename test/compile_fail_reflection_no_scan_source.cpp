// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: the enclosing namespace cannot be captured here

#include <boost/openmethod.hpp>

#if !BOOST_OPENMETHOD_HAS_REFLECTION

// Without reflection there is nothing to check; produce the expected
// diagnostic so the test passes under any configuration.
#error the enclosing namespace cannot be captured here

#else

namespace app {

struct my_registry : boost::openmethod::default_registry::with<> {};

// Only the default template argument of the bare class template - or
// BOOST_OPENMETHOD_REGISTER_CLASSES - can capture the enclosing namespace. With a
// registry as the only argument, there is nothing to scan.
BOOST_OPENMETHOD_REGISTER(
    boost::openmethod::register_classes<^^app::my_registry>);

} // namespace app

#endif

int main() {
    return 0;
}
