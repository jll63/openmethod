// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
#include <boost/openmethod.hpp>

struct Animal {
    virtual ~Animal() {
    }
};

struct Cat : Animal {};

BOOST_OPENMETHOD_OVERRIDE(poke, (boost::openmethod::virtual_ptr<Cat>&), void) {
}
// end::content[]
