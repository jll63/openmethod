// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Tests for reflection-based class registration - `register_classes` and
// `BOOST_OPENMETHOD_REGISTER_CLASSES`. Without a compiler that supports C++26
// reflection there is nothing to test, and the whole file reduces to one test
// case that says so.

#include <sstream>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_util.hpp"

#define BOOST_TEST_MODULE reflection
#include <boost/test/unit_test.hpp>

#if !BOOST_OPENMETHOD_HAS_REFLECTION

BOOST_AUTO_TEST_CASE(reflection_not_supported) {
    BOOST_TEST_MESSAGE("compiler does not support C++26 reflection");
}

#else

#include <memory>

#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>

using namespace boost::openmethod;

// True if `Class` was registered in `Registry`, as seen by the compiler object
// `initialize` returns.
template<class Class, class Registry, class Compiler>
auto registered(const Compiler& comp) -> bool {
    return comp.class_map.find(
               Registry::rtti::type_index(
                   Registry::rtti::template static_type<Class>())) !=
        comp.class_map.end();
}

// =============================================================================
// The macro interface, and a leaf class no signature names

namespace macro_interface {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};
struct Cat : Animal {};

// Never named in a method or an overrider, and never wrapped in a virtual_ptr.
// Only the namespace scan can find it.
struct Bulldog : Dog {};

// Not related to any virtual parameter: must be left alone.
struct Fence {
    virtual ~Fence() = default;
};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat&), std::string) {
    return "hiss";
}

} // namespace macro_interface

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^macro_interface, ^^macro_interface::test_registry>);

BOOST_AUTO_TEST_CASE(macro_interface_dispatch) {
    using namespace macro_interface;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));
    BOOST_TEST((registered<Cat, test_registry>(comp)));
    // Found by the scan, although nothing dispatches on it.
    BOOST_TEST((registered<Bulldog, test_registry>(comp)));
    // Unrelated to every virtual parameter.
    BOOST_TEST((!registered<Fence, test_registry>(comp)));

    Animal animal;
    Dog dog;
    Cat cat;
    Bulldog bulldog;

    BOOST_TEST(poke(animal) == "generic");
    BOOST_TEST(poke(dog) == "bark");
    BOOST_TEST(poke(cat) == "hiss");
    // Bulldog has no overrider of its own; it is registered as derived from
    // Dog, so Dog's overrider applies.
    BOOST_TEST(poke(bulldog) == "bark");
}

// =============================================================================
// The core interface: a method named by an alias, overriders as free functions

namespace core_interface {

struct test_registry : test_registry_<__COUNTER__> {};

struct Node {
    virtual ~Node() = default;
};

struct Literal : Node {};
struct Plus : Node {};

struct BOOST_OPENMETHOD_ID(value);

using value = method<
    BOOST_OPENMETHOD_ID(value), std::string(virtual_ptr<Node, test_registry>),
    test_registry>;

auto value_literal(virtual_ptr<Literal, test_registry>) -> std::string {
    return "literal";
}

auto value_plus(virtual_ptr<Plus, test_registry>) -> std::string {
    return "plus";
}

BOOST_OPENMETHOD_REGISTER(value::override<value_literal, value_plus>);

} // namespace core_interface

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^core_interface, ^^core_interface::test_registry>);

BOOST_AUTO_TEST_CASE(core_interface_dispatch) {
    using namespace core_interface;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Node, test_registry>(comp)));
    BOOST_TEST((registered<Literal, test_registry>(comp)));
    BOOST_TEST((registered<Plus, test_registry>(comp)));

    Literal literal;
    Plus plus;

    BOOST_TEST(
        value::fn(virtual_ptr<Node, test_registry>(literal)) == "literal");
    BOOST_TEST(value::fn(virtual_ptr<Node, test_registry>(plus)) == "plus");
}

// =============================================================================
// A method with no overrider at all

namespace method_only {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

// No overrider, so no registrar object names the method. It is found through
// the alias BOOST_OPENMETHOD declares for it.
BOOST_OPENMETHOD(poke, (virtual_<Animal&>), void, test_registry);

} // namespace method_only

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^method_only, ^^method_only::test_registry>);

BOOST_AUTO_TEST_CASE(method_without_overrider) {
    using namespace method_only;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));
}

// =============================================================================
// Inheritance: virtual, multiple, and inaccessible bases

namespace inheritance {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Herbivore : virtual Animal {};
struct Carnivore : virtual Animal {};
struct Omnivore : Herbivore, Carnivore {};

// Reached only through a private base: the library cannot convert a Stowaway to
// an Animal, so it must not be registered as one.
struct Stowaway : private Animal {};

BOOST_OPENMETHOD(
    meet, (virtual_<Animal&>, virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(meet, (Animal&, Animal&), std::string) {
    return "ignore";
}

BOOST_OPENMETHOD_OVERRIDE(meet, (Carnivore&, Herbivore&), std::string) {
    return "hunt";
}

} // namespace inheritance

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^inheritance, ^^inheritance::test_registry>);

BOOST_AUTO_TEST_CASE(virtual_and_multiple_inheritance) {
    using namespace inheritance;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Herbivore, test_registry>(comp)));
    BOOST_TEST((registered<Carnivore, test_registry>(comp)));
    BOOST_TEST((registered<Omnivore, test_registry>(comp)));
    BOOST_TEST((!registered<Stowaway, test_registry>(comp)));

    Herbivore herbivore;
    Carnivore carnivore;
    Omnivore omnivore;

    BOOST_TEST(meet(herbivore, herbivore) == "ignore");
    BOOST_TEST(meet(carnivore, herbivore) == "hunt");
    // Omnivore is both, and inherits the Carnivore/Herbivore overrider.
    BOOST_TEST(meet(omnivore, omnivore) == "hunt");
}

// =============================================================================
// Repeated inheritance is left out, not rejected
//
// `use_classes` rejects an ambiguous base at compile time, because naming one
// is a mistake in a hand-written list. Here the classes are collected
// mechanically, and a class that happens to have one must not break the build.

namespace repeated_inheritance {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

struct Left : Animal {};
struct Right : Animal {};
// Animal is an ambiguous base: no conversion to it exists.
struct Repeated : Left, Right {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

} // namespace repeated_inheritance

BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^repeated_inheritance, ^^repeated_inheritance::test_registry>);

BOOST_AUTO_TEST_CASE(repeated_inheritance_does_not_break_the_scan) {
    using namespace repeated_inheritance;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));
    BOOST_TEST((registered<Left, test_registry>(comp)));
    BOOST_TEST((registered<Right, test_registry>(comp)));

    Dog dog;
    Left left;
    BOOST_TEST(poke(dog) == "bark");
    BOOST_TEST(poke(left) == "generic");
}

// =============================================================================
// Nested namespaces, smart pointers, and covariant return types

namespace nested {

struct test_registry : test_registry_<__COUNTER__> {};

namespace shapes {

struct Shape {
    virtual ~Shape() = default;
};

namespace round {
struct Circle : Shape {};
} // namespace round

} // namespace shapes

BOOST_OPENMETHOD(
    name, (virtual_ptr<std::shared_ptr<shapes::Shape>, test_registry>),
    std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(
    name, (virtual_ptr<std::shared_ptr<shapes::round::Circle>, test_registry>),
    std::string) {
    return "circle";
}

} // namespace nested

BOOST_OPENMETHOD_REGISTER(register_classes<^^nested, ^^nested::test_registry>);

BOOST_AUTO_TEST_CASE(nested_namespaces_and_smart_pointers) {
    using namespace nested;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<shapes::Shape, test_registry>(comp)));
    BOOST_TEST((registered<shapes::round::Circle, test_registry>(comp)));

    std::shared_ptr<shapes::Shape> circle =
        std::make_shared<shapes::round::Circle>();
    BOOST_TEST(
        name(
            virtual_ptr<std::shared_ptr<shapes::Shape>, test_registry>(
                circle)) == "circle");
}

// =============================================================================
// A base class nothing dispatches on is not registered
//
// Registering it would cost a class_info, a perfect-hash slot and dispatch table
// space, for a class no overrider can ever be selected on.

namespace unused_bases {

struct test_registry : test_registry_<__COUNTER__> {};

// Neither of these is a virtual parameter of any method below.
struct Serializable {
    virtual ~Serializable() = default;
};

struct Named : Serializable {};

struct Animal : Named {};
struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

} // namespace unused_bases

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^unused_bases, ^^unused_bases::test_registry>);

BOOST_AUTO_TEST_CASE(bases_that_take_no_part_in_dispatch_are_left_out) {
    using namespace unused_bases;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));
    // Above the only virtual parameter, so of no use to dispatch.
    BOOST_TEST((!registered<Named, test_registry>(comp)));
    BOOST_TEST((!registered<Serializable, test_registry>(comp)));

    Animal animal;
    Dog dog;
    BOOST_TEST(poke(animal) == "generic");
    BOOST_TEST(poke(dog) == "bark");
}

// =============================================================================
// ... unless another method dispatches on it

namespace shared_bases {

struct test_registry : test_registry_<__COUNTER__> {};

struct Serializable {
    virtual ~Serializable() = default;
};

struct Named : Serializable {};

struct Animal : Named {};
struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

// Named is a virtual parameter here, so it - and the lattice edges through it -
// must be registered after all.
BOOST_OPENMETHOD(label, (virtual_<Named&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(label, (Named&), std::string) {
    return "named";
}

BOOST_OPENMETHOD_OVERRIDE(label, (Dog&), std::string) {
    return "dog";
}

} // namespace shared_bases

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^shared_bases, ^^shared_bases::test_registry>);

BOOST_AUTO_TEST_CASE(a_base_another_method_dispatches_on_is_registered) {
    using namespace shared_bases;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Named, test_registry>(comp)));
    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));
    // Still above every virtual parameter.
    BOOST_TEST((!registered<Serializable, test_registry>(comp)));

    Animal animal;
    Dog dog;
    BOOST_TEST(poke(dog) == "bark");
    // Dog reaches Named through Animal: the edges survive.
    BOOST_TEST(label(dog) == "dog");
    BOOST_TEST(label(animal) == "named");
}

// =============================================================================
// The recorded bases are the direct ones
//
// Reflection knows a class' direct bases, so the registry records those, not the
// whole ancestry. `initialize` derives the lattice from them either way; the
// point is to not ship, instantiate and store what it can work out for itself.

namespace direct_bases {

struct test_registry : test_registry_<__COUNTER__> {};

struct A {
    virtual ~A() = default;
};

struct B : A {};
struct C : B {};
struct D : C {};

BOOST_OPENMETHOD(poke, (virtual_<A&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (A&), std::string) {
    return "A";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (C&), std::string) {
    return "C";
}

} // namespace direct_bases

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^direct_bases, ^^direct_bases::test_registry>);

BOOST_AUTO_TEST_CASE(recorded_bases_are_direct) {
    using namespace direct_bases;

    auto comp = initialize<test_registry>();

    // Each class_info names the class itself, as its own improper base, plus
    // its direct bases - four entries for the whole chain, not ten.
    std::size_t recorded = 0;

    for (auto iter = comp.classes_begin(); iter != comp.classes_end(); ++iter) {
        recorded += iter->last_base - iter->first_base;
    }

    BOOST_TEST(recorded == 7u); // A: 1, B/C/D: 2 each

    // The lattice initialize derives from them is still the full chain.
    auto a = comp.class_map.at(
        test_registry::rtti::type_index(test_registry::rtti::static_type<A>()));
    auto d = comp.class_map.at(
        test_registry::rtti::type_index(test_registry::rtti::static_type<D>()));

    BOOST_TEST(d->direct_bases.size() == 1u);
    BOOST_TEST(d->transitive_bases.size() == 3u);
    BOOST_TEST(a->transitive_derived.size() == 4u);

    A a_obj;
    B b;
    C c;
    D d_obj;
    BOOST_TEST(poke(a_obj) == "A");
    BOOST_TEST(poke(b) == "A");
    BOOST_TEST(poke(c) == "C");
    BOOST_TEST(poke(d_obj) == "C");
}

// =============================================================================
// explicit_class_registration opts out

namespace opted_out {

struct test_registry :
    test_registry_<
        __COUNTER__, policies::explicit_class_registration,
        policies::throw_error_handler> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), void) {
}

} // namespace opted_out

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^opted_out, ^^opted_out::test_registry>);

BOOST_AUTO_TEST_CASE(explicit_class_registration_disables_the_scan) {
    static_assert(!opted_out::test_registry::has_reflected_class_registration);
    // Nothing was registered, so initialize cannot resolve the method's
    // virtual parameter.
    BOOST_CHECK_THROW(initialize<opted_out::test_registry>(), missing_class);
}

// =============================================================================
// Several namespaces in one registration

namespace two_namespaces {

struct test_registry : test_registry_<__COUNTER__> {};

namespace zoo {

struct Animal {
    virtual ~Animal() = default;
};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

} // namespace zoo

namespace pets {

struct Dog : zoo::Animal {};

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

} // namespace pets

} // namespace two_namespaces

BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^two_namespaces::zoo, ^^two_namespaces::pets,
        ^^two_namespaces::test_registry>);

BOOST_AUTO_TEST_CASE(several_namespaces_in_one_registration) {
    using namespace two_namespaces;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<zoo::Animal, test_registry>(comp)));
    BOOST_TEST((registered<pets::Dog, test_registry>(comp)));

    pets::Dog dog;
    BOOST_TEST(zoo::poke(dog) == "bark");
}

// =============================================================================
// With no namespace and no class, the enclosing namespace is scanned

namespace enclosing_macro {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

// Only the registry is named; the macro captures the enclosing namespace.
BOOST_OPENMETHOD_REGISTER_CLASSES(^^enclosing_macro::test_registry);

} // namespace enclosing_macro

BOOST_AUTO_TEST_CASE(macro_scans_the_enclosing_namespace) {
    using namespace enclosing_macro;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));

    Dog dog;
    BOOST_TEST(poke(dog) == "bark");
}

namespace enclosing_helper {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

// The bare class template cannot capture the enclosing namespace on its own;
// `current_namespace()` passes it explicitly.
BOOST_OPENMETHOD_REGISTER(
    register_classes<current_namespace(), ^^enclosing_helper::test_registry>);

} // namespace enclosing_helper

BOOST_AUTO_TEST_CASE(current_namespace_names_the_enclosing_namespace) {
    using namespace enclosing_helper;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));

    Dog dog;
    BOOST_TEST(poke(dog) == "bark");
}

// =============================================================================
// Classes without a namespace: no scan, exactly the listed classes

namespace classes_only {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};
struct Bulldog : Dog {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

} // namespace classes_only

// `Dog` is left out on purpose: only the listed classes are registered, and
// the inheritance lattice is flattened over the gap - `Bulldog`'s recorded
// base is `Animal`.
BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^classes_only::Animal, ^^classes_only::Bulldog,
        ^^classes_only::test_registry>);

BOOST_AUTO_TEST_CASE(listed_classes_without_a_namespace_disable_the_scan) {
    using namespace classes_only;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Bulldog, test_registry>(comp)));
    BOOST_TEST((!registered<Dog, test_registry>(comp)));

    // The flattened lattice records `Animal` as `Bulldog`'s direct base.
    auto bulldog = comp.class_map.at(
        test_registry::rtti::type_index(
            test_registry::rtti::static_type<Bulldog>()));
    BOOST_TEST(bulldog->direct_bases.size() == 1u);

    // The edge is live: the overrider for `Animal` applies to `Bulldog`.
    Bulldog snoopy;
    BOOST_TEST(poke(snoopy) == "generic");
}

// =============================================================================
// A listed class is a root for the scan

namespace listed_root {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

// No method dispatches on `Tool`; only listing it registers it - along with
// `Hammer`, which the scan finds derives from it.
struct Tool {
    virtual ~Tool() = default;
};

struct Hammer : Tool {};

// Unrelated to any root: must be left alone, as always.
struct Fence {
    virtual ~Fence() = default;
};

} // namespace listed_root

BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^listed_root, ^^listed_root::Tool, ^^listed_root::test_registry>);

BOOST_AUTO_TEST_CASE(a_listed_class_roots_the_scan) {
    using namespace listed_root;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Tool, test_registry>(comp)));
    BOOST_TEST((registered<Hammer, test_registry>(comp)));
    BOOST_TEST((!registered<Fence, test_registry>(comp)));
}

// =============================================================================
// no_recurse stops at the listed namespaces

namespace no_recurse {

struct test_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

namespace kennel {

struct Bulldog : Dog {};

} // namespace kennel

} // namespace no_recurse

BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^no_recurse, register_classes_opts::no_recurse,
        ^^no_recurse::test_registry>);

BOOST_AUTO_TEST_CASE(no_recurse_skips_nested_namespaces) {
    using namespace no_recurse;

    auto comp = initialize<test_registry>();

    BOOST_TEST((registered<Animal, test_registry>(comp)));
    BOOST_TEST((registered<Dog, test_registry>(comp)));
    BOOST_TEST((!registered<kennel::Bulldog, test_registry>(comp)));
}

// =============================================================================
// boost is skipped by default; scan_boost brings it back in

namespace boost_gate {

struct default_registry_ : test_registry_<__COUNTER__> {};
struct scan_boost_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, default_registry_);
BOOST_OPENMETHOD_OVERRIDE(poke, (Animal&), std::string) {
    return "generic";
}

BOOST_OPENMETHOD(poke2, (virtual_<Animal&>), std::string, scan_boost_registry);
BOOST_OPENMETHOD_OVERRIDE(poke2, (Animal&), std::string) {
    return "generic";
}

} // namespace boost_gate

namespace boost::om_reflection_test {

struct Stray : boost_gate::Animal {};

} // namespace boost::om_reflection_test

BOOST_OPENMETHOD_REGISTER(
    register_classes<^^::, ^^boost_gate::default_registry_>);

BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^::, register_classes_opts::scan_boost,
        ^^boost_gate::scan_boost_registry>);

BOOST_AUTO_TEST_CASE(boost_is_scanned_only_on_request) {
    using namespace boost_gate;
    using boost::om_reflection_test::Stray;

    auto default_comp = initialize<default_registry_>();
    BOOST_TEST((registered<Animal, default_registry_>(default_comp)));
    BOOST_TEST((!registered<Stray, default_registry_>(default_comp)));

    auto scan_boost_comp = initialize<scan_boost_registry>();
    BOOST_TEST((registered<Animal, scan_boost_registry>(scan_boost_comp)));
    BOOST_TEST((registered<Stray, scan_boost_registry>(scan_boost_comp)));
}

// =============================================================================
// Several registries in one registration

namespace two_registries {

struct first_registry : test_registry_<__COUNTER__> {};
struct second_registry : test_registry_<__COUNTER__> {};

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::string, first_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

} // namespace two_registries

BOOST_OPENMETHOD_REGISTER(
    register_classes<
        ^^two_registries::Animal, ^^two_registries::Dog,
        ^^two_registries::first_registry, ^^two_registries::second_registry>);

BOOST_AUTO_TEST_CASE(several_registries_in_one_registration) {
    using namespace two_registries;

    auto first_comp = initialize<first_registry>();
    BOOST_TEST((registered<Animal, first_registry>(first_comp)));
    BOOST_TEST((registered<Dog, first_registry>(first_comp)));

    auto second_comp = initialize<second_registry>();
    BOOST_TEST((registered<Animal, second_registry>(second_comp)));
    BOOST_TEST((registered<Dog, second_registry>(second_comp)));

    Dog dog;
    BOOST_TEST(poke(dog) == "bark");
}

#endif
