// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::dl_shared[]
// dl_shared.cpp

#include <string>
#include <boost/openmethod.hpp>

#include "animals.hpp"

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Herbivore>, virtual_ptr<Carnivore>), std::string) {
    return "run";
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Carnivore>, virtual_ptr<Herbivore>), std::string) {
    return "hunt";
}
// end::dl_shared[]
