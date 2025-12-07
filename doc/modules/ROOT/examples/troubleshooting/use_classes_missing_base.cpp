// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <iostream>

using namespace boost::openmethod::aliases;

struct Node {
    virtual ~Node() {
    }
};

struct Variable : Node {
    Variable(int value) : val(value) {
    }
    int val;
};

BOOST_OPENMETHOD(value, (virtual_ptr<Node>), int);

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<Variable> node), int) {
    return node->val;
}

BOOST_OPENMETHOD_CLASSES(Node);
BOOST_OPENMETHOD_CLASSES(Variable);

int main() {
    boost::openmethod::initialize();
    Variable var(42);
    std::cout << value(var) << "\n";
}
// end::content[]
