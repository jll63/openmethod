// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// A `virtual_ptr` to the class itself, as a member of that class - a linked
// structure. The class is incomplete where the member is declared, so nothing
// that instantiating `virtual_ptr<Node>` entails may need a complete `Node`.
// Three of its member templates did, through a constraint that gcc evaluates
// when the class is instantiated, and libstdc++ 16 makes `is_polymorphic` of
// an incomplete type a hard error where 13 let it through.

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE virtual_ptr_self_referential
#include <boost/test/unit_test.hpp>

#include <string>

using namespace boost::openmethod;

struct Node {
    virtual ~Node() = default;
    virtual_ptr<Node> next;
};

struct Leaf : Node {};

BOOST_OPENMETHOD_CLASSES(Node, Leaf);

BOOST_OPENMETHOD(name, (virtual_ptr<Node>), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (virtual_ptr<Node>), std::string) {
    return "node";
}

BOOST_OPENMETHOD_OVERRIDE(name, (virtual_ptr<Leaf>), std::string) {
    return "leaf";
}

BOOST_AUTO_TEST_CASE(virtual_ptr_self_referential) {
    initialize();

    Leaf leaf;
    Node node;

    // The assignment from `Other&` whose constraint was evaluated too early...
    node.next = leaf;
    BOOST_TEST(name(node.next) == "leaf");

    // ...and the implicit copies, for which overload resolution tries the
    // same member template with `Other` = `const virtual_ptr<Node>`.
    Node copy = node;
    BOOST_TEST(name(copy.next) == "leaf");
    copy.next = nullptr;
    copy = node;
    BOOST_TEST(name(copy.next) == "leaf");
}
