// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::main[]

// dl_main.cpp

#include "animals.hpp"

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include <iostream>

using namespace boost::openmethod::aliases;

struct Cow : Herbivore {};
struct Wolf : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Animal, Herbivore, Cow, Wolf, Carnivore);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Animal>, virtual_ptr<Animal>), std::string) {
    return "greet";
}

// end::main[]

// tag::before_dlopen[]
auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<Animal> gracie(new Cow());
    std::unique_ptr<Animal> willy(new Wolf());

    std::cout << "Gracie meets Willy -> " << meet(*gracie, *willy) << "\n";
    std::cout << "Willy meets Gracie -> " << meet(*willy, *gracie) << "\n";

    return 0;
}
