// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::main[]

// dl_main.cpp

#define MAIN

#include <cstring>
#include <iostream>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

#include <boost/dll/shared_library.hpp>

#include "dl.hpp"
#include <boost/dll/runtime_symbol_info.hpp>

BOOST_OPENMETHOD_CLASSES(Animal, Herbivore, Cow, Wolf, Carnivore, dynamic);

BOOST_OPENMETHOD_OVERRIDE(
    encounter, (dyn_vptr<Animal>, dyn_vptr<Animal>), std::string) {
    return "ignore\n";
}

// end::main[]

// tag::before_dlopen[]
auto main() -> int {
    using namespace boost::openmethod;

        try {

        initialize<dynamic>();

        std::cout << "Before loading library\n";

        auto gracie = make_unique_virtual<Cow, dynamic>();
        auto willy = make_unique_virtual<Wolf, dynamic>();

        std::cout << "Gracie encounters Willy -> "
                << encounter(gracie, willy); // ignore
        std::cout << "Willy encounters Gracie -> "
                << encounter(willy, gracie); // ignore
        // end::before_dlopen[]

        // tag::dlopen[]
        boost::dll::shared_library lib(
            boost::dll::program_location().parent_path() / "dl_shared.dll",
            boost::dll::load_mode::rtld_now);

        std::cout << "\nAfter loading library\n";

        initialize<dynamic>();


        lib.get<void()>("test")();



        std::cout << "Willy encounters Gracie -> "
                << encounter(willy, gracie); // hunt
        // end::dlopen[]

        // tag::after_dlclose[]
        lib.unload();

        std::cout << "\nAfter unloading library\n";

        initialize<dynamic>();

        std::cout << "Gracie encounters Willy -> "
                << encounter(gracie, willy); // clang: ignore, g++: run
        std::cout << "Willy encounters Gracie -> "
                << encounter(willy, gracie); // clang: ignore, g++: hunt
        // end::after_dlclose[]
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
