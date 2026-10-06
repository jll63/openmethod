// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// static_rtti and named_static_rtti have no dynamic_type. A bad_call still
// reports the class of each argument: the one whose v-table the virtual_ptr
// points to.

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/static_rtti.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include <sstream>
#include <string>

using namespace boost::openmethod;

struct static_registry :
    registry<policies::static_rtti, policies::throw_error_handler> {};

struct named_registry :
    registry<policies::named_static_rtti, policies::throw_error_handler> {};

struct Animal {};

struct Dog : Animal {};

struct Cat : Animal {};

namespace with_static_rtti {

using R = static_registry;

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, R);

BOOST_OPENMETHOD(speak, (virtual_ptr<Animal, R>), void);

BOOST_OPENMETHOD_OVERRIDE(speak, (virtual_ptr<Cat, R>), void) {
}

BOOST_OPENMETHOD(meet, (virtual_ptr<Animal, R>, virtual_ptr<Animal, R>), void);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Dog, R>, virtual_ptr<Animal, R>), void) {
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Animal, R>, virtual_ptr<Dog, R>), void) {
}

} // namespace with_static_rtti

namespace with_named_static_rtti {

using R = named_registry;

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, R);

BOOST_OPENMETHOD(speak, (virtual_ptr<Animal, R>), void);

BOOST_OPENMETHOD_OVERRIDE(speak, (virtual_ptr<Cat, R>), void) {
}

BOOST_OPENMETHOD(meet, (virtual_ptr<Animal, R>, virtual_ptr<Animal, R>), void);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Dog, R>, virtual_ptr<Animal, R>), void) {
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Animal, R>, virtual_ptr<Dog, R>), void) {
}

} // namespace with_named_static_rtti

// Checks that `call` reports `Error`, and that every argument is reported as a
// Dog - the class of the object, not Animal, the class of the parameter.
template<class Registry, class Error, class Call>
void check_error(Call call, std::size_t arity) {
    using rtti = typename Registry::rtti;

    try {
        call();
        BOOST_FAIL("should have thrown");
    } catch (const Error& error) {
        BOOST_TEST(error.arity == arity);

        for (std::size_t i = 0; i < arity; ++i) {
            BOOST_TEST(
                (rtti::type_index(error.types[i]) ==
                    rtti::type_index(rtti::template static_type<Dog>())));
        }
    } catch (...) {
        BOOST_FAIL("wrong exception");
    }
}

BOOST_AUTO_TEST_CASE(static_rtti_errors) {
    using namespace with_static_rtti;

    initialize<R>();
    Dog dog;

    check_error<R, no_overrider>([&] { speak(final_virtual_ptr<R>(dog)); }, 1);
    check_error<R, no_overrider>(
        [&] { speak(make_unique_virtual<Dog, R>()); }, 1);
    check_error<R, ambiguous_call>(
        [&] { meet(final_virtual_ptr<R>(dog), final_virtual_ptr<R>(dog)); }, 2);
}

BOOST_AUTO_TEST_CASE(named_static_rtti_errors) {
    using namespace with_named_static_rtti;

    initialize<R>();
    Dog dog;

    check_error<R, no_overrider>([&] { speak(final_virtual_ptr<R>(dog)); }, 1);
    check_error<R, no_overrider>(
        [&] { speak(make_unique_virtual<Dog, R>()); }, 1);
    check_error<R, ambiguous_call>(
        [&] { meet(final_virtual_ptr<R>(dog), final_virtual_ptr<R>(dog)); }, 2);

    try {
        speak(final_virtual_ptr<R>(dog));
    } catch (const no_overrider& error) {
        std::ostringstream os;
        R::rtti::type_name(error.types[0], os);
        auto name = os.str();
        BOOST_TEST(name.substr(name.size() - 3) == "Dog");
    }
}
