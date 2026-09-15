// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: declared its registry affinity after it was first mentioned

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct zoo_registry : default_registry {};

struct Cat;

struct Animal {
    virtual ~Animal() = default;
    friend auto boost_openmethod_registry(Animal*) -> zoo_registry;
};

// Cat is declared but not defined, so lookup sees neither its base class nor
// the affinity it inherits: this is a `virtual_ptr<Cat, default_registry>`,
// and the answer is remembered. Naming the registry here would be right.
using early = virtual_ptr<Cat>;

struct Cat : Animal {};

// Registering Cat asks again, now that it is complete, and the two answers
// disagree.
BOOST_OPENMETHOD_CLASSES(Animal, Cat, zoo_registry);

int main() {
    return 0;
}
