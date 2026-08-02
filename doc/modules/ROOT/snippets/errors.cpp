// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <sstream>
#include <string>
#include <variant>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

// Everything between here and the first example is harness, kept out of the
// tagged regions: what the reference pages show is the mistake and the
// operation that reports it, which is all a reader needs.

// Redirects std::cerr for the duration of a scope, so that the test can check
// what the error handler below wrote.
struct capture_cerr {
    std::ostringstream captured;
    std::streambuf* previous = std::cerr.rdbuf(captured.rdbuf());

    ~capture_cerr() {
        std::cerr.rdbuf(previous);
    }

    auto str() const -> std::string {
        return captured.str();
    }
};

// Thrown only to unwind out of an example: the library calls `abort` as soon as
// the error handler returns, and an error handler may prevent that only by
// throwing.
struct reported {};

// Reports the error the way the default handler does, but on std::cerr. The
// `output` policy writes to the C `stderr` stream, which a streambuf redirect
// cannot intercept, so `capture_cerr` would see nothing otherwise.
template<class Registry>
auto report_on_cerr() -> void {
    Registry::error_handler::set([](const auto& error) {
        std::visit(
            [](auto&& e) { e.template write<Registry>(std::cerr); }, error);
        std::cerr << "\n";
        throw reported{};
    });
}

struct Animal {
    virtual ~Animal() {
    }
};
struct Dog : Animal {};
struct Bulldog : Dog {};

// Each example below registers its classes in a registry of its own, so that
// one deliberate mistake does not affect the others.
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

// `runtime_checks` is named explicitly rather than left to
// BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS, which only a debug build defines:
// the class missing from a *call* is caught by that policy, so without it the
// last example below would proceed on a v-table pointer that was never set up.
template<int N>
using reporting = default_registry::with<marker<N>, policies::runtime_checks>;

namespace missing_parameter_class {

struct missing_parameter : reporting<1> {};

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

struct missing_overrider : reporting<2> {};

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

struct missing_argument : reporting<3> {};

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

struct unrelated_classes : reporting<4> {};

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

struct related_classes : reporting<5> {};

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
        capture_cerr cerr;
        report_on_cerr<missing_parameter>();

        try {
            // tag::missing_class_in_method_init[]
            // error: unknown class Animal
            initialize<missing_parameter>();
            // end::missing_class_in_method_init[]
        } catch (const reported&) {
        }

        BOOST_TEST(cerr.str().find("Animal") != std::string::npos);
    }

    {
        using namespace missing_overrider_class;
        capture_cerr cerr;
        report_on_cerr<missing_overrider>();

        try {
            // tag::missing_class_in_overrider_init[]
            // error: unknown class Dog
            initialize<missing_overrider>();
            // end::missing_class_in_overrider_init[]
        } catch (const reported&) {
        }

        BOOST_TEST(cerr.str().find("Dog") != std::string::npos);
    }

    {
        using namespace missing_call_class;
        initialize<missing_argument>();
        capture_cerr cerr;
        report_on_cerr<missing_argument>();

        try {
            // tag::missing_class_in_call_use[]
            Bulldog hector;

            // error: unknown class Bulldog
            poke(virtual_ptr<Animal, missing_argument>(hector));
            // end::missing_class_in_call_use[]
        } catch (const reported&) {
        }

        BOOST_TEST(cerr.str().find("Bulldog") != std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(missing_base_errors) {
    {
        using namespace unrelated_registration;
        capture_cerr cerr;
        report_on_cerr<unrelated_classes>();

        try {
            // tag::missing_base_init[]
            // error: missing base Animal -<| Dog
            initialize<unrelated_classes>();
            // end::missing_base_init[]
        } catch (const reported&) {
        }

        BOOST_TEST(cerr.str().find("missing base") != std::string::npos);
    }

    {
        using namespace related_registration;
        initialize<related_classes>();

        Dog snoopy;
        poke(virtual_ptr<Animal, related_classes>(snoopy));
    }
}
