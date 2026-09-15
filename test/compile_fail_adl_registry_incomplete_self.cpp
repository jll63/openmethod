// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: declared its registry affinity after it was first mentioned

#include <string>

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct zoo_registry : default_registry {};

struct Node {
    virtual ~Node() = default;

    // The class is incomplete inside its own body, and has declared nothing
    // when the member is reached: the typedef below comes too late, and so
    // would a hidden friend. Move the declaration above the member.
    virtual_ptr<Node> next;

    using boost_openmethod_registry = zoo_registry;
};

// A method over Node asks again, and the two answers disagree.
BOOST_OPENMETHOD(name, (virtual_<const Node&>), std::string);

int main() {
    Node node;
    name(node);

    return 0;
}
