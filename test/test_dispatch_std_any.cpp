// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <any>

#include <boost/openmethod.hpp>
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
// pass virtual args as const std::any& (const ref)

static_assert(
    detail::has_vptr<
        virtual_traits<const std::any&, default_registry>, const std::any&>);

MAKE_CLASSES();

BOOST_OPENMETHOD(name, (virtual_<const std::any&>), std::string);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const std::string& name), std::string) {
    return name;
}

BOOST_OPENMETHOD_OVERRIDE(name, (const int& value), std::string) {
    std::ostringstream os;
    os << value << " the integer";
    return os.str();
}

BOOST_AUTO_TEST_CASE(std_any_by_const_ref) {
    initialize(trace());

    const std::any spot(Dog{"Spot"});
    const std::any felix(std::string{"Felix the cat"});
    const std::any answer(42);

    BOOST_TEST(name(spot) == "Spot the dog");
    BOOST_TEST(name(felix) == "Felix the cat");
    BOOST_TEST(name(answer) == "42 the integer");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as std::any& (mutable ref)

static_assert(detail::has_vptr<
              virtual_traits<std::any&, default_registry>, const std::any&>);

MAKE_CLASSES();

BOOST_OPENMETHOD(bump, (virtual_<std::any&>), std::string);

// BOOST_OPENMETHOD_OVERRIDE cannot express this. It locates the method by
// checking that the overrider's parameter types can be passed to the method's
// forwarder (see enable_forwarder and the guide function in macros.hpp), and
// `Dog&` does not convert to `std::any&`. A temporary `std::any` binds to
// `const std::any&` and to `std::any&&`, which is why the other two reference
// categories can use the macro; nothing binds to a mutable lvalue reference.
// Register directly via method<...>::override<Fn> instead - the primitive the
// macro itself expands to.

using bump_method =
    BOOST_OPENMETHOD_TYPE(bump, (virtual_<std::any&>), std::string);

auto bump_dog(Dog& dog) -> std::string {
    dog.name += " Jr.";
    return dog.name + " the dog";
}

auto bump_string(std::string& name) -> std::string {
    name += "!";
    return name;
}

auto bump_int(int& value) -> std::string {
    ++value;
    std::ostringstream os;
    os << value << " the integer";
    return os.str();
}

BOOST_OPENMETHOD_REGISTER(bump_method::override<bump_dog>);
BOOST_OPENMETHOD_REGISTER(bump_method::override<bump_string>);
BOOST_OPENMETHOD_REGISTER(bump_method::override<bump_int>);

BOOST_AUTO_TEST_CASE(std_any_by_mutable_ref) {
    initialize(trace());

    std::any spot(Dog{"Spot"});
    std::any felix(std::string{"Felix the cat"});
    std::any answer(41);

    BOOST_TEST(bump(spot) == "Spot Jr. the dog");
    BOOST_TEST(std::any_cast<const Dog&>(spot).name == "Spot Jr.");

    BOOST_TEST(bump(felix) == "Felix the cat!");
    BOOST_TEST(std::any_cast<const std::string&>(felix) == "Felix the cat!");

    BOOST_TEST(bump(answer) == "42 the integer");
    BOOST_TEST(std::any_cast<const int&>(answer) == 42);
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args as std::any&& (xvalue ref)

static_assert(detail::has_vptr<
              virtual_traits<std::any&&, default_registry>, const std::any&>);

MAKE_CLASSES();

BOOST_OPENMETHOD(steal, (virtual_<std::any&&>), std::string);

BOOST_OPENMETHOD_OVERRIDE(steal, (Dog && dog), std::string) {
    Dog stolen(std::move(dog));
    return stolen.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(steal, (std::string && name), std::string) {
    std::string stolen(std::move(name));
    return stolen;
}

BOOST_OPENMETHOD_OVERRIDE(steal, (int&& value), std::string) {
    std::ostringstream os;
    os << value << " the integer";
    return os.str();
}

BOOST_AUTO_TEST_CASE(std_any_by_xvalue_ref) {
    initialize(trace());

    std::any spot(Dog{"Spot"});
    BOOST_TEST(steal(std::move(spot)) == "Spot the dog");
    // the overrider moved the name out; the `any` still owns the Dog
    BOOST_TEST(spot.has_value());
    BOOST_TEST(std::any_cast<const Dog&>(spot).name == "");

    std::any felix(std::string{"Felix the cat"});
    BOOST_TEST(steal(std::move(felix)) == "Felix the cat");
    BOOST_TEST(felix.has_value());
    BOOST_TEST(std::any_cast<const std::string&>(felix) == "");

    // moving an int copies it
    std::any answer(42);
    BOOST_TEST(steal(std::move(answer)) == "42 the integer");
    BOOST_TEST(std::any_cast<const int&>(answer) == 42);
}
} // namespace BOOST_OPENMETHOD_GENSYM
