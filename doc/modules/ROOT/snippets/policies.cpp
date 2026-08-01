// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>
#include <boost/openmethod/policies/vptr_map.hpp>

#include <stdexcept>
#include <variant>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

struct Animal {
    virtual ~Animal() = default;
};
struct Cat : Animal {};
struct Dog : Animal {};

// The registries below each get their own copy of the classes and of `trick`.
// Only `Dog` has an overrider, so calling `trick` on a `Cat` reaches the
// registry's error handler.

namespace std_rtti_demo {

// tag::std_rtti[]
struct dynamic_registry : registry<
                              policies::std_rtti, policies::fast_perfect_hash,
                              policies::vptr_vector> {};
// end::std_rtti[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, dynamic_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, dynamic_registry>), std::string,
    dynamic_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, dynamic_registry>), std::string) {
    return "spin";
}

} // namespace std_rtti_demo

namespace vptr_vector_demo {

// tag::vptr_vector[]
// `fast_perfect_hash` turns the type ids into small indices; without it the
// vector is indexed by the type id itself, which `std_rtti` makes a pointer
struct vector_registry : registry<
                             policies::std_rtti, policies::fast_perfect_hash,
                             policies::vptr_vector> {};
// end::vptr_vector[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, vector_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, vector_registry>), std::string,
    vector_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, vector_registry>), std::string) {
    return "spin";
}

} // namespace vptr_vector_demo

namespace vptr_map_demo {

// tag::vptr_map[]
struct map_registry : registry<policies::std_rtti, policies::vptr_map<>> {};
// end::vptr_map[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, map_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, map_registry>), std::string, map_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, map_registry>), std::string) {
    return "spin";
}

} // namespace vptr_map_demo

namespace fast_perfect_hash_demo {

// tag::fast_perfect_hash[]
// `vptr_vector` indexes by the type id unless a `type_hash` policy maps it to
// a small integer first. With `std_rtti`, where a type id is a pointer, that
// makes the difference between a vector of a few entries and one that cannot
// be allocated at all.
struct hashed_registry : registry<
                             policies::std_rtti, policies::fast_perfect_hash,
                             policies::vptr_vector> {};
// end::fast_perfect_hash[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, hashed_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, hashed_registry>), std::string,
    hashed_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, hashed_registry>), std::string) {
    return "spin";
}

} // namespace fast_perfect_hash_demo

namespace stderr_output_demo {

// tag::stderr_output[]
struct noisy_registry
    : registry<
          policies::std_rtti, policies::fast_perfect_hash,
          policies::vptr_vector, policies::default_error_handler,
          policies::stderr_output> {};
// end::stderr_output[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, noisy_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, noisy_registry>), std::string, noisy_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, noisy_registry>), std::string) {
    return "spin";
}

} // namespace stderr_output_demo

namespace default_error_handler_demo {

// tag::default_error_handler_registry[]
struct handled_registry
    : registry<
          policies::std_rtti, policies::fast_perfect_hash,
          policies::vptr_vector, policies::default_error_handler,
          policies::stderr_output> {};
// end::default_error_handler_registry[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, handled_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, handled_registry>), std::string,
    handled_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, handled_registry>), std::string) {
    return "spin";
}

} // namespace default_error_handler_demo

namespace throw_error_handler_demo {

// tag::throw_error_handler_registry[]
struct throwing_registry
    : registry<
          policies::std_rtti, policies::fast_perfect_hash,
          policies::vptr_vector, policies::throw_error_handler> {};
// end::throw_error_handler_registry[]

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, throwing_registry);

BOOST_OPENMETHOD(
    trick, (virtual_ptr<Animal, throwing_registry>), std::string,
    throwing_registry);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (virtual_ptr<Dog, throwing_registry>), std::string) {
    return "spin";
}

} // namespace throw_error_handler_demo

BOOST_AUTO_TEST_CASE(rtti_and_storage) {
    {
        using namespace std_rtti_demo;
        initialize<dynamic_registry>();

        // tag::std_rtti_dispatch[]
        Dog snoopy;
        Animal& animal = snoopy;

        BOOST_TEST(
            trick(virtual_ptr<Animal, dynamic_registry>(animal)) == "spin");
        // end::std_rtti_dispatch[]
    }

    {
        using namespace vptr_vector_demo;
        initialize<vector_registry>();

        Dog snoopy;
        BOOST_TEST(
            trick(virtual_ptr<Animal, vector_registry>(snoopy)) == "spin");
    }

    {
        using namespace vptr_map_demo;
        initialize<map_registry>();

        Dog snoopy;
        BOOST_TEST(trick(virtual_ptr<Animal, map_registry>(snoopy)) == "spin");
    }

    {
        using namespace fast_perfect_hash_demo;
        initialize<hashed_registry>();

        Dog snoopy;
        BOOST_TEST(
            trick(virtual_ptr<Animal, hashed_registry>(snoopy)) == "spin");
    }

    {
        using namespace stderr_output_demo;
        initialize<noisy_registry>();

        Dog snoopy;
        BOOST_TEST(
            trick(virtual_ptr<Animal, noisy_registry>(snoopy)) == "spin");
    }
}

BOOST_AUTO_TEST_CASE(error_handlers) {
    {
        using namespace default_error_handler_demo;
        initialize<handled_registry>();

        // tag::default_error_handler_set[]
        handled_registry::error_handler::set([](const auto& error) {
            if (std::holds_alternative<no_overrider>(error)) {
                throw std::runtime_error("not implemented");
            }
        });

        Cat felix;

        BOOST_CHECK_THROW(
            trick(virtual_ptr<Animal, handled_registry>(felix)),
            std::runtime_error);
        // end::default_error_handler_set[]
    }

    {
        using namespace throw_error_handler_demo;
        initialize<throwing_registry>();

        // tag::throw_error_handler_catch[]
        Cat felix;

        BOOST_CHECK_THROW(
            trick(virtual_ptr<Animal, throwing_registry>(felix)), no_overrider);
        // end::throw_error_handler_catch[]
    }
}
