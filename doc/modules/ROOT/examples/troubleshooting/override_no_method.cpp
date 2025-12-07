// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// up to: postfix_boost_openmethod_guide

// tag::content[]
#include <boost/openmethod.hpp>
#include <iostream>

using namespace boost::openmethod::aliases;

struct Node {
    virtual ~Node() = default;
};

struct Plus : Node {};

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<Plus>, std::ostream& os), void) {
}
// end::content[]
