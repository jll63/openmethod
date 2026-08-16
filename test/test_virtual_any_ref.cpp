// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <any>
#include <string>
#include <utility>

#include <boost/any.hpp>
#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_any.hpp>
#include <boost/openmethod/interop/std_any.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

#define MAKE_CLASSES()                                                         \
    struct Dog {                                                               \
        std::string name;                                                      \
    };                                                                         \
                                                                               \
    use_std_any_types<Dog, std::string, int> BOOST_OPENMETHOD_GENSYM;

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// const handle: virtual_any_ref<const std::any>

MAKE_CLASSES();

BOOST_OPENMETHOD(name, (virtual_any_ref<const std::any>), std::string);

// A plain value does not convert to a virtual_any_ref, so
// BOOST_OPENMETHOD_OVERRIDE cannot locate the method for overriders that
// take the contained value. Register them with the core API instead - the
// primitive the macro itself expands to.

using name_method =
    BOOST_OPENMETHOD_TYPE(name, (virtual_any_ref<const std::any>), std::string);

auto name_dog(const Dog& dog) -> std::string {
    return dog.name + " the dog";
}

auto name_string(const std::string& name) -> std::string {
    return name;
}

BOOST_OPENMETHOD_REGISTER(name_method::override<name_dog>);
BOOST_OPENMETHOD_REGISTER(name_method::override<name_string>);

// The catch-all overrider takes the handle itself, by value; the macro
// locates the method, since the conversion is the identity.
BOOST_OPENMETHOD_OVERRIDE(
    name, (virtual_any_ref<const std::any> va), std::string) {
    return va.get().has_value() ? "something" : "nothing";
}

BOOST_AUTO_TEST_CASE(virtual_any_ref_const) {
    initialize(trace());

    // from an `any`: the v-table pointer is looked up from the dynamic
    // type of the contained value
    const std::any spot_any(Dog{"Spot"});
    virtual_any_ref<const std::any> spot = spot_any;
    BOOST_TEST(spot.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(&spot.get() == &spot_any);
    BOOST_TEST(name(spot) == "Spot the dog");

    // an `any` lvalue converts to a (temporary) handle at the call site
    std::any felix_any(std::string{"Felix the cat"});
    BOOST_TEST(name(felix_any) == "Felix the cat");

    // from a virtual_any: the v-table pointer is copied - no lookup
    const virtual_std_any rex = Dog{"Rex"};
    virtual_any_ref<const std::any> rex_ref = rex;
    BOOST_TEST(rex_ref.vptr() == rex.vptr());
    BOOST_TEST(name(rex_ref) == "Rex the dog");

    // a mutable handle converts to a const one
    std::any answer_any(42);
    virtual_any_ref<std::any> answer = answer_any;
    virtual_any_ref<const std::any> const_answer = answer;
    BOOST_TEST(const_answer.vptr() == answer.vptr());

    // `int` is registered, but has no specific overrider: the catch-all,
    // registered for the `std::any` root, applies
    BOOST_TEST(name(const_answer) == "something");

    // copying a handle copies the two words; both refer to the same `any`
    auto copy = spot;
    BOOST_TEST(&copy.get() == &spot_any);
    BOOST_TEST(copy.vptr() == spot.vptr());
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// mutable handle: virtual_any_ref<std::any>

MAKE_CLASSES();

BOOST_OPENMETHOD(bump, (virtual_any_ref<std::any>), std::string);

using bump_method =
    BOOST_OPENMETHOD_TYPE(bump, (virtual_any_ref<std::any>), std::string);

auto bump_dog(Dog& dog) -> std::string {
    dog.name += " Jr.";
    return dog.name + " the dog";
}

auto bump_int(int& value) -> std::string {
    ++value;
    return "bumped";
}

BOOST_OPENMETHOD_REGISTER(bump_method::override<bump_dog>);
BOOST_OPENMETHOD_REGISTER(bump_method::override<bump_int>);

BOOST_AUTO_TEST_CASE(virtual_any_ref_mutable) {
    initialize(trace());

    // the handle refers to the `any`; mutations reach the referent
    std::any spot_any(Dog{"Spot"});
    BOOST_TEST(bump(spot_any) == "Spot Jr. the dog");
    BOOST_TEST(std::any_cast<const Dog&>(spot_any).name == "Spot Jr.");

    std::any answer_any(41);
    virtual_any_ref<std::any> answer = answer_any;
    BOOST_TEST(bump(answer) == "bumped");
    BOOST_TEST(std::any_cast<int>(answer_any) == 42);

    // referring to a virtual_any: mutations reach the owner's value
    virtual_std_any rex = Dog{"Rex"};
    virtual_any_ref<std::any> rex_ref = rex;
    BOOST_TEST(rex_ref.vptr() == rex.vptr());
    BOOST_TEST(bump(rex_ref) == "Rex Jr. the dog");
    BOOST_TEST(std::any_cast<const Dog&>(rex.get()).name == "Rex Jr.");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// boost::any: virtual_any_ref is generic over the `any` type

struct Dog {
    std::string name;
};

use_boost_any_types<Dog, std::string, int> BOOST_OPENMETHOD_GENSYM;

BOOST_OPENMETHOD(name, (virtual_any_ref<const boost::any>), std::string);

using name_method = BOOST_OPENMETHOD_TYPE(
    name, (virtual_any_ref<const boost::any>), std::string);

auto name_dog(const Dog& dog) -> std::string {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_REGISTER(name_method::override<name_dog>);

BOOST_OPENMETHOD_OVERRIDE(
    name, (virtual_any_ref<const boost::any> va), std::string) {
    return va.get().empty() ? "nothing" : "something";
}

BOOST_OPENMETHOD(bump, (virtual_any_ref<boost::any>), std::string);

using bump_method =
    BOOST_OPENMETHOD_TYPE(bump, (virtual_any_ref<boost::any>), std::string);

auto bump_dog(Dog& dog) -> std::string {
    dog.name += " Jr.";
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_REGISTER(bump_method::override<bump_dog>);

BOOST_AUTO_TEST_CASE(virtual_any_ref_boost_any) {
    initialize(trace());

    const boost::any spot_any(Dog{"Spot"});
    virtual_any_ref<const boost::any> spot = spot_any;
    BOOST_TEST(spot.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(name(spot) == "Spot the dog");

    // `int` is registered, but has no specific overrider: the catch-all,
    // registered for the `boost::any` root, applies
    boost::any answer_any(42);
    BOOST_TEST(name(answer_any) == "something");

    // mutations through a mutable handle reach the referent
    boost::any rex_any(Dog{"Rex"});
    BOOST_TEST(bump(rex_any) == "Rex Jr. the dog");
    BOOST_TEST(boost::any_cast<const Dog&>(rex_any).name == "Rex Jr.");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// indirect vptrs

struct Dog {
    std::string name;
};

use_std_any_types<Dog, std::string, int, indirect_registry>
    BOOST_OPENMETHOD_GENSYM;

using name_method = method<
    struct name_id,
    std::string(virtual_any_ref<const std::any, indirect_registry>),
    indirect_registry>;

auto name_dog(const Dog& dog) -> std::string {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_REGISTER(name_method::override<name_dog>);

BOOST_AUTO_TEST_CASE(virtual_any_ref_indirect_vptr) {
    initialize<indirect_registry>();

    std::any spot_any(Dog{"Spot"});
    virtual_any_ref<const std::any, indirect_registry> spot = spot_any;
    BOOST_TEST(spot.vptr() == indirect_registry::static_vptr<Dog>);
    BOOST_TEST(name_method::fn(spot) == "Spot the dog");
}
} // namespace BOOST_OPENMETHOD_GENSYM
