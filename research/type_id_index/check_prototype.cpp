// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.
//
// Properties of the prototypes themselves, over and above re-running the
// library's own tests against them. Compiled once per RESEARCH_POLICY variant,
// including the `fph` control, so every assertion here is one the stock policy
// satisfies too.

#include "registry.hpp"

#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE research_check_prototype
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

namespace bom = boost::openmethod;

namespace {

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};
struct Cat : Animal {};
struct Bulldog : Dog {};

BOOST_OPENMETHOD(speak, (bom::virtual_<const Animal&>), std::string);

BOOST_OPENMETHOD_OVERRIDE(speak, (const Animal&), std::string) {
    return "...";
}

BOOST_OPENMETHOD_OVERRIDE(speak, (const Dog&), std::string) {
    return "bark";
}

BOOST_OPENMETHOD_OVERRIDE(speak, (const Cat&), std::string) {
    return "meow";
}

BOOST_OPENMETHOD_OVERRIDE(speak, (const Bulldog&), std::string) {
    return "growl";
}

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, Bulldog);

} // namespace

// The map must be injective over the registered classes, and dispatch must
// select the same overrider whichever policy computes the index.
BOOST_AUTO_TEST_CASE(dispatches_and_is_injective) {
    bom::initialize();

    Animal animal;
    Dog dog;
    Cat cat;
    Bulldog bulldog;

    BOOST_CHECK_EQUAL(speak(animal), "...");
    BOOST_CHECK_EQUAL(speak(dog), "bark");
    BOOST_CHECK_EQUAL(speak(cat), "meow");
    BOOST_CHECK_EQUAL(speak(bulldog), "growl");

    // Same objects, reached through a base reference: this is the path that
    // goes through the type_id -> index map.
    const Animal& as_dog = dog;
    const Animal& as_cat = cat;
    const Animal& as_bulldog = bulldog;

    BOOST_CHECK_EQUAL(speak(as_dog), "bark");
    BOOST_CHECK_EQUAL(speak(as_cat), "meow");
    BOOST_CHECK_EQUAL(speak(as_bulldog), "growl");
}

// initialize() must be idempotent: the second call rebuilds the tables from
// the same registrations and must leave dispatch working.
BOOST_AUTO_TEST_CASE(initialize_is_idempotent) {
    bom::initialize();
    bom::initialize();

    Dog dog;
    const Animal& animal = dog;
    BOOST_CHECK_EQUAL(speak(animal), "bark");
}

// The claim the whole scheme rests on: two distinct type_info objects are at
// least 16 bytes apart, so (ti - min) / d is injective for every d <= 16.
// Checked here over every type_info this translation unit can name, which is
// what a registry would be handed.
BOOST_AUTO_TEST_CASE(type_infos_are_at_least_16_bytes_apart) {
    std::vector<std::uintptr_t> addresses{
        reinterpret_cast<std::uintptr_t>(&typeid(Animal)),
        reinterpret_cast<std::uintptr_t>(&typeid(Dog)),
        reinterpret_cast<std::uintptr_t>(&typeid(Cat)),
        reinterpret_cast<std::uintptr_t>(&typeid(Bulldog)),
    };

    std::sort(addresses.begin(), addresses.end());

    for (std::size_t i = 1; i < addresses.size(); ++i) {
        BOOST_TEST(addresses[i] - addresses[i - 1] >= 16u);
    }

    for (std::size_t divisor : {std::size_t(8), std::size_t(16)}) {
        std::set<std::uintptr_t> indexes;

        for (auto address : addresses) {
            indexes.insert((address - addresses.front()) / divisor);
        }

        BOOST_TEST(indexes.size() == addresses.size());
    }
}
