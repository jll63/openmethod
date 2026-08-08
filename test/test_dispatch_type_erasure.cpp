// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <utility>

#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/is_empty.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

namespace te = boost::type_erasure;
using namespace boost::openmethod;

// `relaxed` implies typeid_<>, on which typeid_of and any_cast - thus
// dispatch - rely; no explicit typeid_<> needed.
using Concept = boost::mpl::vector<te::copy_constructible<>, te::relaxed>;
using erased = te::any<Concept>;
using erased_ref = te::any<Concept, te::_self&>;
using erased_cref = te::any<Concept, const te::_self&>;

static_assert(detail::has_vptr<
              virtual_traits<const erased&, default_registry>, const erased&>);

#define MAKE_CLASSES()                                                         \
    struct Dog {                                                               \
        std::string name;                                                      \
    };                                                                         \
                                                                               \
    BOOST_OPENMETHOD_REGISTER(                                                 \
        use_type_erasure_types<erased, Dog, std::string, int>);

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as const any& (const ref)

MAKE_CLASSES();

BOOST_OPENMETHOD(name, (virtual_<const erased&>), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const std::string& name), std::string) {
    return name;
}

// A catch-all overrider may take the `any` itself; the argument is passed
// through unchanged.
BOOST_OPENMETHOD_OVERRIDE(name, (const erased& arg), std::string) {
    return te::is_empty(arg) ? "nothing" : "something";
}

BOOST_AUTO_TEST_CASE(type_erasure_by_const_ref) {
    initialize(trace());

    const erased spot(Dog{"Spot"});
    const erased felix(std::string{"Felix the cat"});
    const erased answer(42);

    BOOST_TEST(name(spot) == "Spot the dog");
    BOOST_TEST(name(felix) == "Felix the cat");
    // `int` is registered but has no specific overrider: the catch-all,
    // registered for the root, applies
    BOOST_TEST(name(answer) == "something");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as any& (mutable ref)

MAKE_CLASSES();

BOOST_OPENMETHOD(bump, (virtual_<erased&>), std::string);

// BOOST_OPENMETHOD_OVERRIDE cannot express this: a temporary `any` binds
// to `const any&` and to `any&&`, but nothing binds to a mutable lvalue
// reference. Register directly via method<...>::override<Fn> instead -
// the primitive the macro itself expands to.

using bump_method =
    BOOST_OPENMETHOD_TYPE(bump, (virtual_<erased&>), std::string);

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

BOOST_AUTO_TEST_CASE(type_erasure_by_mutable_ref) {
    initialize(trace());

    erased spot(Dog{"Spot"});
    BOOST_TEST(bump(spot) == "Spot Jr. the dog");
    // the mutation is visible through the `any`
    BOOST_TEST(te::any_cast<const Dog&>(spot).name == "Spot Jr.");

    erased answer(41);
    BOOST_TEST(bump(answer) == "bumped");
    BOOST_TEST(te::any_cast<int>(answer) == 42);
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as any&& (xvalue ref)

MAKE_CLASSES();

BOOST_OPENMETHOD(steal, (virtual_<erased&&>), std::string);

// boost::type_erasure::any_cast has no rvalue overload; the trait moves
// the result of a mutable-reference cast, because the `any` owns its
// value.
BOOST_OPENMETHOD_OVERRIDE(steal, (Dog && dog), std::string) {
    Dog stolen(std::move(dog));
    return stolen.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(steal, (std::string && name), std::string) {
    std::string stolen(std::move(name));
    return stolen;
}

BOOST_AUTO_TEST_CASE(type_erasure_by_xvalue_ref) {
    initialize(trace());

    erased spot(Dog{"Spot"});
    BOOST_TEST(steal(std::move(spot)) == "Spot the dog");
    // the overrider moved the name out; the `any` still owns the Dog
    BOOST_TEST(!te::is_empty(spot));
    BOOST_TEST(te::any_cast<const Dog&>(spot).name == "");

    erased felix(std::string{"Felix the cat"});
    BOOST_TEST(steal(std::move(felix)) == "Felix the cat");
    BOOST_TEST(te::any_cast<const std::string&>(felix) == "");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as any<C, _self&> - the mutable reference-wrapper
// flavor - by value

MAKE_CLASSES();

BOOST_OPENMETHOD(poke, (virtual_<erased_ref>), std::string);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog & dog), std::string) {
    dog.name += "!";
    return dog.name;
}

BOOST_OPENMETHOD_OVERRIDE(poke, (int& value), std::string) {
    ++value;
    return "poked";
}

BOOST_AUTO_TEST_CASE(type_erasure_ref_wrapper_by_value) {
    initialize(trace());

    // the wrapper is a cheap handle; mutations reach the referents
    Dog snoopy{"Snoopy"};
    int count = 41;

    BOOST_TEST(poke(erased_ref(snoopy)) == "Snoopy!");
    BOOST_TEST(snoopy.name == "Snoopy!");

    BOOST_TEST(poke(erased_ref(count)) == "poked");
    BOOST_TEST(count == 42);
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as any<C, const _self&> - the const reference-wrapper
// flavor - by value

MAKE_CLASSES();

BOOST_OPENMETHOD(name, (virtual_<erased_cref>), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

// the catch-all receives a copy of the wrapper - still a cheap handle
BOOST_OPENMETHOD_OVERRIDE(name, (erased_cref arg), std::string) {
    return te::is_empty(arg) ? "nothing" : "something";
}

BOOST_AUTO_TEST_CASE(type_erasure_cref_wrapper_by_value) {
    initialize(trace());

    Dog snoopy{"Snoopy"};
    const int count = 42;

    BOOST_TEST(name(erased_cref(snoopy)) == "Snoopy the dog");
    BOOST_TEST(name(erased_cref(count)) == "something");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// virtual_any over a type_erasure any: the v-table pointer is looked up
// once, at construction - or set statically when the contained type is
// known - and dispatch does not hash typeid_of on every call

MAKE_CLASSES();

BOOST_OPENMETHOD(name, (const virtual_any<erased>&), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_AUTO_TEST_CASE(type_erasure_virtual_any) {
    initialize(trace());

    // from an `any`: runtime lookup via typeid_of
    erased spot_any(Dog{"Spot"});
    virtual_any<erased> spot = spot_any;
    BOOST_TEST(spot.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(name(spot) == "Spot the dog");

    // from a value: the v-table pointer is set statically
    virtual_any<erased> rex = Dog{"Rex"};
    BOOST_TEST(rex.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(name(rex) == "Rex the dog");

    auto snoopy = make_any_virtual<Dog, erased>(Dog{"Snoopy"});
    BOOST_TEST(snoopy.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(name(snoopy) == "Snoopy the dog");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// indirect vptrs

struct Dog {
    std::string name;
};

BOOST_OPENMETHOD_REGISTER(
    use_type_erasure_types<erased, Dog, std::string, int, indirect_registry>);

using name_method = method<
    struct name_id, std::string(virtual_<const erased&>), indirect_registry>;

auto name_dog(const Dog& dog) -> std::string {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_REGISTER(name_method::override<name_dog>);

BOOST_AUTO_TEST_CASE(type_erasure_indirect_vptr) {
    initialize<indirect_registry>();

    const erased spot(Dog{"Spot"});
    BOOST_TEST(name_method::fn(spot) == "Spot the dog");

    virtual_any<erased, indirect_registry> rex = Dog{"Rex"};
    BOOST_TEST(rex.vptr() == indirect_registry::static_vptr<Dog>);
    BOOST_TEST(name_method::fn(rex.get()) == "Rex the dog");
}
} // namespace BOOST_OPENMETHOD_GENSYM
