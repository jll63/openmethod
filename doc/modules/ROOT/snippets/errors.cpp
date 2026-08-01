// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

struct Animal {
    virtual ~Animal() {
    }
};
struct Dog : Animal {};
struct Bulldog : Dog {};

// Each example below registers its classes in a registry of its own, so that
// one deliberate mistake does not affect the others. They all throw rather
// than abort, which is what `throw_error_handler` is for.
//
// Registries that derive from the same `registry<...>` specialization share
// one state, so deriving all of them from a single alias would pool the
// registrations and mask the mistakes. A marker policy carrying an integer
// gives each a distinct base -- the same device as `test_registry_` in
// test/test_util.hpp.

struct marker_category {
    using category = marker_category;
};

template<int N>
struct marker final : marker_category {
    template<class Registry>
    struct fn {};
};

template<int N>
using throwing =
    default_registry::with<marker<N>, policies::throw_error_handler>;

namespace missing_parameter_class {

struct missing_parameter : throwing<1> {};

// tag::missing_class_in_method[]
BOOST_OPENMETHOD_CLASSES(Dog, missing_parameter); // Animal is missing

BOOST_OPENMETHOD(
    poke, (virtual_ptr<Animal, missing_parameter>), void, missing_parameter);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog, missing_parameter>), void) { /* ... */
}
// end::missing_class_in_method[]

} // namespace missing_parameter_class

namespace missing_overrider_class {

struct missing_overrider : throwing<2> {};

// tag::missing_class_in_overrider[]
BOOST_OPENMETHOD_CLASSES(Animal, missing_overrider); // Dog is missing

BOOST_OPENMETHOD(
    poke, (virtual_ptr<Animal, missing_overrider>), void, missing_overrider);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog, missing_overrider>), void) { /* ... */
}
// end::missing_class_in_overrider[]

} // namespace missing_overrider_class

namespace missing_call_class {

struct missing_argument : throwing<3> {};

// tag::missing_class_in_call[]
BOOST_OPENMETHOD_CLASSES(Animal, Dog, missing_argument); // Bulldog is missing

BOOST_OPENMETHOD(
    poke, (virtual_ptr<Animal, missing_argument>), void, missing_argument);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog, missing_argument>), void) { /* ... */
}
// end::missing_class_in_call[]

} // namespace missing_call_class

namespace unrelated_registration {

struct unrelated_classes : throwing<4> {};

// tag::missing_base[]
// registered separately, so the inheritance is never seen
BOOST_OPENMETHOD_CLASSES(Animal, unrelated_classes);
BOOST_OPENMETHOD_CLASSES(Dog, unrelated_classes);

BOOST_OPENMETHOD(
    poke, (virtual_ptr<Animal, unrelated_classes>), void, unrelated_classes);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog, unrelated_classes>), void) { /* ... */
}
// end::missing_base[]

} // namespace unrelated_registration

namespace related_registration {

struct related_classes : throwing<5> {};

// tag::missing_base_fix[]
BOOST_OPENMETHOD_CLASSES(Animal, Dog, related_classes);
// end::missing_base_fix[]

BOOST_OPENMETHOD(
    poke, (virtual_ptr<Animal, related_classes>), void, related_classes);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog, related_classes>), void) { /* ... */
}

} // namespace related_registration

BOOST_AUTO_TEST_CASE(missing_class_errors) {
    {
        using namespace missing_parameter_class;
        // tag::missing_class_in_method_init[]
        BOOST_CHECK_THROW(initialize<missing_parameter>(), missing_class);
        // end::missing_class_in_method_init[]
    }

    {
        using namespace missing_overrider_class;
        // tag::missing_class_in_overrider_init[]
        BOOST_CHECK_THROW(initialize<missing_overrider>(), missing_class);
        // end::missing_class_in_overrider_init[]
    }

    {
        using namespace missing_call_class;
        initialize<missing_argument>();

        // tag::missing_class_in_call_use[]
        Bulldog hector;

        BOOST_CHECK_THROW(
            poke(virtual_ptr<Animal, missing_argument>(hector)), missing_class);
        // end::missing_class_in_call_use[]
    }
}

BOOST_AUTO_TEST_CASE(missing_base_errors) {
    {
        using namespace unrelated_registration;
        // tag::missing_base_init[]
        BOOST_CHECK_THROW(initialize<unrelated_classes>(), missing_base);
        // end::missing_base_init[]
    }

    {
        using namespace related_registration;
        initialize<related_classes>();

        Dog snoopy;
        poke(virtual_ptr<Animal, related_classes>(snoopy));
    }
}
