// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::main[]

// dl_main.cpp

#include "animals.hpp"

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>

#include <boost/dll/shared_library.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

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

#ifndef LIBRARY_NAME
#ifdef _MSC_VER
#define LIBRARY_NAME "shared.dll"
#else
#define LIBRARY_NAME "libshared.so"
#endif
#endif

auto main() -> int {
    using namespace boost::openmethod::aliases;

    std::cout << "Before loading the shared library.\n";
    initialize();

    {
        auto gracie = make_unique_virtual<Cow>();
        auto willy = make_unique_virtual<Wolf>();

        std::cout << "cow meets wolf -> " << meet(*gracie, *willy) << "\n";
        std::cout << "wolf meets cow -> " << meet(*willy, *gracie) << "\n";
    }

    boost::dll::shared_library lib(
        boost::dll::program_location().parent_path() / LIBRARY_NAME,
        boost::dll::load_mode::rtld_now);

    std::cout << "\nAfter loading the shared library.\n";

    initialize();

    {
        auto gracie = make_unique_virtual<Cow>();
        auto willy = make_unique_virtual<Wolf>();

        std::cout << "cow meets wolf -> " << meet(*gracie, *willy) << "\n";
        std::cout << "wolf meets cow -> " << meet(*willy, *gracie) << "\n";

        auto make_tiger = lib.get<Animal*()>("make_tiger");
        std::unique_ptr<Animal> hobbes{make_tiger()};
        std::cout << "cow meets tiger -> " << meet(*gracie, *hobbes) << "\n";
    }

    std::cout << "\nAfter unloading the shared library.\n";
    lib.unload();

    initialize();

    {
        std::unique_ptr<Animal> gracie(new Cow());
        std::unique_ptr<Animal> willy(new Wolf());

        std::cout << "cow meets wolf -> " << meet(*gracie, *willy) << "\n";
        std::cout << "wolf meets cow -> " << meet(*willy, *gracie) << "\n";
    }

    return 0;
}
