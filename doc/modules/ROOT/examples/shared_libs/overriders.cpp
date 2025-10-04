// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// overriders.cpp


#include "animals.hpp"

using namespace boost::openmethod::aliases;

#ifdef _WIN32
using meet_type = BOOST_OPENMETHOD_TYPE(
    meet, (virtual_ptr<Animal>, virtual_ptr<Animal>), std::string);
//extern _declspec(dllimport) meet_type meet_type::fn;

// using meet_method = BOOST_OPENMETHOD_TYPE(
//     meet, (virtual_ptr<Animal>, virtual_ptr<Animal>), std::string);
// extern __declspec(dllimport) meet_method meet_method::fn;
#endif

#include <iostream>

using namespace boost::openmethod;

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Herbivore>, virtual_ptr<Carnivore>), std::string) {
    std::cerr << &default_registry::classes << "\n";
    return "run";
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Carnivore>, virtual_ptr<Herbivore>), std::string) {
    std::cerr << &default_registry::classes << "\n";
    return "hunt";
}
// end::content[]
