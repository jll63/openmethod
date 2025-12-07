// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// up to: no matching

// tag::content[]
#include <boost/openmethod.hpp>
#include <iostream>

using namespace boost::openmethod::aliases;

struct Node {};

struct Plus : Node {};

BOOST_OPENMETHOD(postfix, (virtual_ptr<Node>, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<Plus>, std::ostream& os), void) {
}

int main() {
    Plus p;
    postfix(p, std::cout);
}
// end::content[]
