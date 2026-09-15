// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: conflicting registry affinities

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct zoo_registry : default_registry {};

struct Animal {
    virtual ~Animal() = default;
    friend auto boost_openmethod_registry(Animal*) -> zoo_registry;
};

// An affinity declared for the default registry is a declared affinity all the
// same: Widget does not yield to Animal the way a class that declares nothing
// would.
struct Widget {
    virtual ~Widget() = default;
    friend auto boost_openmethod_registry(Widget*) -> default_registry;
};

BOOST_OPENMETHOD(
    collide, (virtual_<const Animal&>, virtual_<const Widget&>), void);

int main() {
    Animal animal;
    Widget widget;
    collide(animal, widget);

    return 0;
}
