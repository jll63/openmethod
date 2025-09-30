// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::main[]

// dl_main.cpp

#include "animals.hpp"

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include <boost/dll/shared_library.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

#include <iostream>

using namespace boost::openmethod::aliases;

BOOST_OPENMETHOD_CLASSES(Animal, Herbivore, Cow, Wolf, Carnivore);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Animal>, virtual_ptr<Animal>), std::string) {
    return "greet";
}

// end::main[]

// tag::before_dlopen[]
auto main(int argc, const char* argv[]) -> int {
    boost::dll::shared_library lib;

    if (argc >= 2 && std::string(argv[1]) == "reality") {
        lib.load(
            boost::dll::program_location().parent_path() / "libjungle.so",
            boost::dll::load_mode::rtld_now);
        std::cout << "Nature is just another word for jungle!\n";
    }

    boost::openmethod::initialize();

    Animal&& gracie = Cow();
    Animal&& willy = Wolf();

    std::cout << "Gracie meets Willy -> " << meet(gracie, willy) << "\n";
    std::cout << "Willy meets Gracie -> " << meet(willy, gracie) << "\n";

    return 0;
}
