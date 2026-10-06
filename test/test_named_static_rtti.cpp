// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

struct named_registry;
#define BOOST_OPENMETHOD_DEFAULT_REGISTRY named_registry

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/static_rtti.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

struct named_registry :
    boost::openmethod::registry<
        boost::openmethod::policies::named_static_rtti> {};

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include "test_classes.hpp"

#include <sstream>
#include <string>
#include <string_view>

struct Animal {};

struct Dog : Animal {};

struct Cat : Animal {};

namespace {

struct Stray : Animal {};

} // namespace

using namespace boost::openmethod::aliases;

BOOST_OPENMETHOD_TEST_CLASSES(Animal, Dog, Cat);

BOOST_OPENMETHOD_CLASSES(Animal, Stray);

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>, std::ostream& os), void) {
    os << "bark";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat>, std::ostream& os), void) {
    os << "hiss";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Stray>, std::ostream& os), void) {
    os << "growl";
}

using rtti = named_registry::rtti;

template<class Class>
auto key() {
    return rtti::type_index(rtti::static_type<Class>());
}

template<class Class>
auto name() -> std::string {
    std::ostringstream os;
    rtti::type_name(rtti::static_type<Class>(), os);

    return os.str();
}

auto ends_with(std::string_view s, std::string_view suffix) -> bool {
    return s.size() >= suffix.size() &&
        s.substr(s.size() - suffix.size()) == suffix;
}

BOOST_AUTO_TEST_CASE(names) {
    // MSVC prints `struct Dog`.
    BOOST_TEST(ends_with(name<Dog>(), "Dog"));
    BOOST_TEST(ends_with(name<Cat>(), "Cat"));
}

BOOST_AUTO_TEST_CASE(keys) {
    BOOST_TEST((key<Dog>() == key<Dog>()));
    BOOST_TEST((key<Dog>() != key<Cat>()));
    BOOST_TEST((key<Dog>() < key<Cat>() || key<Cat>() < key<Dog>()));

    // A class at namespace scope is identified by its name alone...
    BOOST_TEST(key<Dog>().local == nullptr);

    // ...one in an anonymous namespace by its type_id as well, on every
    // compiler.
    BOOST_TEST(key<Stray>().local == rtti::static_type<Stray>());
}

BOOST_AUTO_TEST_CASE(local_classes) {
    struct Local {};
    auto first = key<Local>();

    {
        struct Local {};
        auto second = key<Local>();

#if BOOST_OPENMETHOD_HAS_REFLECTION ||                                         \
    (defined(__GNUC__) && !defined(__clang__))
        // GCC prints both as `local_classes::test_method()::Local`, and marks
        // them as local; reflection does not need marks.
        BOOST_TEST(first.local != nullptr);
        BOOST_TEST((first != second));
#else
        // Clang and MSVC leave no mark; see the warning in the documentation
        // of named_static_rtti.
        (void)first;
        (void)second;
#endif
    }
}

BOOST_AUTO_TEST_CASE(dispatch) {
    boost::openmethod::initialize();

    {
        std::stringstream os;
        poke(make_unique_virtual<Dog>(), os);
        BOOST_TEST(os.str() == "bark");
    }
    {
        std::stringstream os;
        poke(make_unique_virtual<Cat>(), os);
        BOOST_TEST(os.str() == "hiss");
    }
    {
        std::stringstream os;
        poke(make_unique_virtual<Stray>(), os);
        BOOST_TEST(os.str() == "growl");
    }
}

BOOST_OPENMETHOD_TEST_REGISTER_CLASSES();
