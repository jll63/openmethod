// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

namespace polymorphic_classes {

// tag::polymorphic_classes[]
struct Animal {
    virtual ~Animal() = default;
};
struct Dog : Animal {};
struct Cat : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat);
// end::polymorphic_classes[]

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), std::string);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog> animal), std::string) {
    return "bark";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat> animal), std::string) {
    return "hiss";
}

} // namespace polymorphic_classes

namespace non_polymorphic_classes {

// tag::non_polymorphic_classes[]
// polymorphism not required
struct Animal {};
struct Cat : Animal {};
struct Dog : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog);
// end::non_polymorphic_classes[]

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), std::string);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog> animal), std::string) {
    return "bark";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat> animal), std::string) {
    return "hiss";
}

} // namespace non_polymorphic_classes

BOOST_AUTO_TEST_CASE(virtual_ptr_examples) {
    // tag::initialize[]
    initialize();
    // end::initialize[]

    {
        using namespace non_polymorphic_classes;
        poke(make_unique_virtual<Dog>()); // for coverage
    }

    {
        using namespace polymorphic_classes;
        // tag::assign_nullptr[]
        Dog snoopy;
        virtual_ptr<Animal> p(snoopy);

        p = nullptr;

        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
        // end::assign_nullptr[]
    }

    {
        using namespace polymorphic_classes;

        // tag::cast[]
        Dog snoopy;
        virtual_ptr<Animal> animal(snoopy);

        auto dog = animal.cast<Dog>();

        BOOST_TEST(dog.get() == &snoopy);
        BOOST_TEST(dog.vptr() == animal.vptr());
        // end::cast[]
    }

    {
        using namespace non_polymorphic_classes;

        // tag::final_virtual_ptr[]
        Dog snoopy;
        virtual_ptr<Animal> animal = final_virtual_ptr(snoopy);
        BOOST_TEST(poke(animal) == "bark");

        Cat felix;
        animal = final_virtual_ptr(felix);
        BOOST_TEST(poke(animal) == "hiss");
        // end::final_virtual_ptr[]
    }
}
