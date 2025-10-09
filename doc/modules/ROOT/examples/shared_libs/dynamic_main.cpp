// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef LIBRARY_NAME
#ifdef _MSC_VER
#define LIBRARY_NAME "shared.dll"
#else
#define LIBRARY_NAME "libshared.so"
#endif
#endif

// tag::before[]
// dynamic_main.cpp

#include "animals.hpp"

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <iostream>
#include <memory>

using namespace boost::openmethod::aliases;


// decltype(boost_openmethod_storage_class(
//     std::declval<BOOST_OPENMETHOD_ID(meet)&>(),
//     std::declval<boost::openmethod::virtual_ptr<Animal>>(),
//     std::declval<boost::openmethod::virtual_ptr<Animal>>(),
//     std::declval<std::string>()
// )) x = 1;

// static_assert(
//     std::is_same_v<
//         boost::openmethod::detail::storage_class<
//             BOOST_OPENMETHOD_TYPE(
//                 meet, (
//                     boost::openmethod::virtual_ptr<Animal>,
//                     boost::openmethod::virtual_ptr<Animal>),
//                 std::string)>,
//     boost::openmethod::dll_export>);

// BOOST_OPENMETHOD_TYPE(
//     meet,
//     (boost::openmethod::virtual_ptr<Animal>,
//     boost::openmethod::virtual_ptr<Animal>),
//     std::string
// )::storage_type x = 1;

static_assert(
    std::is_same_v<
        BOOST_OPENMETHOD_TYPE(
            meet,
            (boost::openmethod::virtual_ptr<Animal>,
            boost::openmethod::virtual_ptr<Animal>),
            std::string
        )::storage_type, boost::openmethod::dll_export>);

struct Cow : Herbivore {};
struct Wolf : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Animal, Herbivore, Cow, Wolf, Carnivore);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Animal>, virtual_ptr<Animal>), std::string) {
    return "greet";
}

// tag::load[]
int main() {
    // end::load[]
    // end::unload[]
    std::cout << "Before loading the shared library.\n";

    initialize();

    std::cout << "cow meets wolf -> "
              << meet(*std::make_unique<Cow>(), *std::make_unique<Wolf>())
              << "\n"; // greet
    std::cout << "wolf meets cow -> "
              << meet(*std::make_unique<Wolf>(), *std::make_unique<Cow>())
              << "\n"; // greet

    // to be continued...
    // end::before[]
    // tag::load[]
    // ...

    std::cout << "\nAfter loading the shared library.\n";

    boost::dll::shared_library lib(
        boost::dll::program_location().parent_path() / LIBRARY_NAME,
        boost::dll::load_mode::rtld_now);
    initialize();

    std::cout << "cow meets wolf -> "
              << meet(*std::make_unique<Cow>(), *std::make_unique<Wolf>())
              << "\n"; // run
    std::cout << "wolf meets cow -> "
              << meet(*std::make_unique<Wolf>(), *std::make_unique<Cow>())
              << "\n"; // hunt

    auto make_tiger = lib.get<Animal*()>("make_tiger");
    std::cout << "cow meets tiger -> "
              << meet(
                     *std::make_unique<Cow>(),
                     *std::unique_ptr<Animal>(make_tiger()))
              << "\n"; // hunt
                       // end::load[]

    // tag::unload[]
    // ...

    std::cout << "\nAfter unloading the shared library.\n";

    lib.unload();
    initialize();

    std::cout << "cow meets wolf -> "
              << meet(*std::make_unique<Cow>(), *std::make_unique<Wolf>())
              << "\n"; // greet
    std::cout << "wolf meets cow -> "
              << meet(*std::make_unique<Wolf>(), *std::make_unique<Cow>())
              << "\n"; // greet
    // tag::before[]
    // tag::load[]
    // tag::unload[]

    return 0;
}
// end::before[]
// end::load[]
// end::unload[]
