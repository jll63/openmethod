// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_util.hpp"

#define BOOST_TEST_MODULE slot_allocator
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

using class_ = detail::generic_compiler::class_;
using cc_method = detail::generic_compiler::method;
using overrider = detail::generic_compiler::overrider;

auto operator<<(std::ostream& os, const class_* cls) -> std::ostream& {
    return os
        << reinterpret_cast<const std::type_info*>(cls->ci[0]->type)->name();
}

std::string empty = "{}";

template<template<typename...> class Container, typename T>
auto str(const Container<T>& container) {
    std::ostringstream os;
    os << "{";
    const char* sep = "";

    for (const auto& item : container) {
        os << sep << item;
        sep = ", ";
    }

    os << "}";

    return os.str();
}

template<typename... Ts>
auto sstr(Ts... args) {
    std::vector<class_*> vec{args...};
    std::sort(vec.begin(), vec.end());

    return str(vec);
}

template<typename T>
auto sstr(const std::unordered_set<T>& container) {
    return sstr(std::vector<T>(container.begin(), container.end()));
}

template<typename T, typename Compiler>
auto get_class(const Compiler& comp) {
    // Key through the rtti policy rather than with a raw typeid: the policy
    // decides what identifies a class (std_rtti uses the mangled name, see
    // std_rtti::type_index), and only it knows how to derive that from a type.
    return comp.class_map.at(
        boost::openmethod::default_registry::rtti::type_index(&typeid(T)));
}

/*
A   B
 \ / \
 AB   D
 |    |
 C    E
  \  /
   F
*/

struct A {
    virtual ~A() {
    }
};

struct B {
    virtual ~B() {
    }
};

struct AB : A, B {};

struct C : AB {};

struct D : B {};

struct E : D {};

struct F : C, E {};

// ============================================================================
// Test use_classes.

BOOST_AUTO_TEST_CASE(test_use_classes_linear) {
    struct Base {
        virtual ~Base() = default;
    };

    struct D1 : Base {};
    struct D2 : D1 {};
    struct D3 : D2 {};
    struct D4 : D3 {};
    struct D5 : D4 {};

    struct registry : test_registry_<__COUNTER__> {};

    BOOST_OPENMETHOD_CLASSES(Base, D1, D2, D3, registry);
    BOOST_OPENMETHOD_CLASSES(D2, D3, registry);
    BOOST_OPENMETHOD_CLASSES(D3, D4, registry);
    BOOST_OPENMETHOD_CLASSES(D4, D5, D3, registry);

    auto comp = initialize<registry>();

    auto base = get_class<Base>(comp);
    auto d1 = get_class<D1>(comp);
    auto d2 = get_class<D2>(comp);
    auto d3 = get_class<D3>(comp);
    auto d4 = get_class<D4>(comp);
    auto d5 = get_class<D5>(comp);

    BOOST_CHECK_EQUAL(sstr(base->direct_bases), empty);
    BOOST_CHECK_EQUAL(sstr(base->direct_derived), sstr(d1));
    BOOST_CHECK_EQUAL(
        sstr(base->transitive_derived), sstr(base, d1, d2, d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d1->direct_derived), sstr(d2));
    BOOST_CHECK_EQUAL(sstr(d1->direct_bases), sstr(base));
    BOOST_CHECK_EQUAL(sstr(d1->transitive_derived), sstr(d1, d2, d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d2->direct_derived), sstr(d3));
    BOOST_CHECK_EQUAL(sstr(d2->direct_bases), sstr(d1));
    BOOST_CHECK_EQUAL(sstr(d2->transitive_derived), sstr(d2, d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d3->direct_derived), sstr(d4));
    BOOST_CHECK_EQUAL(sstr(d3->direct_bases), sstr(d2));
    BOOST_CHECK_EQUAL(sstr(d3->transitive_derived), sstr(d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d4->direct_derived), sstr(d5));
    BOOST_CHECK_EQUAL(sstr(d4->direct_bases), sstr(d3));
    BOOST_CHECK_EQUAL(sstr(d4->transitive_derived), sstr(d4, d5));
}

// The lattice must not depend on the order in which classes are registered.
// Every record below carries exactly one, genuinely direct, base; only the order
// of the calls differs from `test_use_classes_linear`, with D3's own ancestry
// registered last.
BOOST_AUTO_TEST_CASE(test_use_classes_derived_before_base) {
    struct Base {
        virtual ~Base() = default;
    };

    struct D1 : Base {};
    struct D2 : D1 {};
    struct D3 : D2 {};
    struct D4 : D3 {};
    struct D5 : D4 {};

    struct registry : test_registry_<__COUNTER__> {};

    BOOST_OPENMETHOD_CLASSES(D3, D4, registry);
    BOOST_OPENMETHOD_CLASSES(D4, D5, registry);
    BOOST_OPENMETHOD_CLASSES(Base, D1, D2, D3, registry);

    auto comp = initialize<registry>();

    auto base = get_class<Base>(comp);
    auto d1 = get_class<D1>(comp);
    auto d2 = get_class<D2>(comp);
    auto d3 = get_class<D3>(comp);
    auto d4 = get_class<D4>(comp);
    auto d5 = get_class<D5>(comp);

    BOOST_CHECK_EQUAL(sstr(base->direct_bases), empty);
    BOOST_CHECK_EQUAL(sstr(d1->direct_bases), sstr(base));
    BOOST_CHECK_EQUAL(sstr(d2->direct_bases), sstr(d1));
    BOOST_CHECK_EQUAL(sstr(d3->direct_bases), sstr(d2));
    BOOST_CHECK_EQUAL(sstr(d4->direct_bases), sstr(d3));
    // D3 is an *indirect* base of D5, and must not appear here.
    BOOST_CHECK_EQUAL(sstr(d5->direct_bases), sstr(d4));

    BOOST_CHECK_EQUAL(sstr(d5->transitive_bases), sstr(base, d1, d2, d3, d4));
    BOOST_CHECK_EQUAL(sstr(d4->transitive_bases), sstr(base, d1, d2, d3));

    BOOST_CHECK_EQUAL(sstr(base->direct_derived), sstr(d1));
    BOOST_CHECK_EQUAL(sstr(d3->direct_derived), sstr(d4));
    BOOST_CHECK_EQUAL(sstr(d4->direct_derived), sstr(d5));
    BOOST_CHECK_EQUAL(
        sstr(base->transitive_derived), sstr(base, d1, d2, d3, d4, d5));
}

BOOST_AUTO_TEST_CASE(test_use_classes_diamond) {
    using test_registry = test_registry_<__COUNTER__>;
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, AB, C, D, E, test_registry>);

    std::vector<class_*> actual, expected;

    auto comp = initialize<test_registry>();

    auto a = get_class<A>(comp);
    auto b = get_class<B>(comp);
    auto ab = get_class<AB>(comp);
    auto c = get_class<C>(comp);
    auto d = get_class<D>(comp);
    auto e = get_class<E>(comp);

    // -----------------------------------------------------------------------
    // A
    BOOST_REQUIRE_EQUAL(sstr(a->direct_bases), empty);
    BOOST_REQUIRE_EQUAL(sstr(a->direct_derived), sstr(ab));
    BOOST_REQUIRE_EQUAL(sstr(a->transitive_derived), sstr(a, ab, c));

    // -----------------------------------------------------------------------
    // B
    BOOST_REQUIRE_EQUAL(sstr(b->direct_bases), empty);
    BOOST_REQUIRE_EQUAL(sstr(b->direct_derived), sstr(ab, d));
    BOOST_REQUIRE_EQUAL(sstr(b->transitive_derived), sstr(b, ab, c, d, e));

    // -----------------------------------------------------------------------
    // AB
    BOOST_REQUIRE_EQUAL(sstr(ab->direct_bases), sstr(a, b));
    BOOST_REQUIRE_EQUAL(sstr(ab->direct_derived), sstr(c));
    BOOST_REQUIRE_EQUAL(sstr(ab->transitive_derived), sstr(ab, c));

    // -----------------------------------------------------------------------
    // C
    BOOST_REQUIRE_EQUAL(sstr(c->direct_bases), sstr(ab));
    BOOST_REQUIRE_EQUAL(sstr(c->direct_derived), empty);
    BOOST_REQUIRE_EQUAL(sstr(c->transitive_derived), sstr(c));

    // -----------------------------------------------------------------------
    // D
    BOOST_REQUIRE_EQUAL(sstr(d->direct_bases), sstr(b));
    BOOST_REQUIRE_EQUAL(sstr(d->direct_derived), sstr(e));
    BOOST_REQUIRE_EQUAL(sstr(d->transitive_derived), sstr(d, e));

    // -----------------------------------------------------------------------
    // E
    BOOST_REQUIRE_EQUAL(sstr(e->direct_bases), sstr(d));
    BOOST_REQUIRE_EQUAL(sstr(e->direct_derived), empty);
    BOOST_REQUIRE_EQUAL(sstr(e->transitive_derived), sstr(e));
}

/// ============================================================================
// Test slot allocation.
//
// The order in which classes are registered is not under a program's control:
// it is the order of static construction across translation units, and across
// modules. The allocator must be correct in every order, and the tests must
// not assume one. A hierarchy is described as a list of registrations, one
// `use_classes` per class with its direct bases, and each test registers them
// in many orders: every permutation for small hierarchies, a fixed sample for
// larger ones. What is checked is what holds in every order: no two parameters
// share a slot in any v-table, every v-table is as tight as its slots allow,
// and the total size of the v-tables is what the expectations say. For every
// hierarchy below, the harness found the total to be the same in every order;
// the comments give what the old allocator, before issue #19, produced over
// the same orders.

auto check(const detail::generic_compiler::method* method)
    -> const detail::generic_compiler::method* {
    if (method) {
        return method;
    }

    BOOST_FAIL("method not found");

    return nullptr;
}

template<int>
struct M;

// A unary method dispatching on CLASS, bound to a local reference.
#define ADD_METHOD(CLASS)                                                      \
    auto& BOOST_PP_CAT(m_, CLASS) =                                            \
        method<CLASS, auto(virtual_<CLASS&>)->void, test_registry>::fn;

#define ADD_METHOD_N(CLASS, N)                                                 \
    auto& BOOST_PP_CAT(BOOST_PP_CAT(m_, CLASS), N) =                           \
        method<M<N>, auto(virtual_<CLASS&>)->void, test_registry>::fn;

// A unary method dispatching on CLASS, with no name to refer to it by.
#define USE_METHOD(CLASS)                                                      \
    (void)&method<CLASS, auto(virtual_<CLASS&>)->void, test_registry>::fn

#define USE_METHOD_N(CLASS, N)                                                 \
    (void)&method<M<N>, auto(virtual_<CLASS&>)->void, test_registry>::fn

// A binary method dispatching on CLASS1 and CLASS2.
#define USE_METHOD2(CLASS1, CLASS2)                                            \
    (void)&method<                                                             \
        M<__LINE__>, auto(virtual_<CLASS1&>, virtual_<CLASS2&>)->void,         \
        test_registry>::fn

// A class registration that the test constructs, and destroys, when it
// decides to. Not named `register_classes`: that is the library's C++26 API.
struct registration {
    virtual ~registration() = default;
};

template<class... Classes>
struct registered : registration {
    use_classes<Classes...> classes;
};

using registration_factory = std::unique_ptr<registration> (*)();

template<class... Classes>
auto registration_of() -> std::unique_ptr<registration> {
    return std::make_unique<registered<Classes...>>();
}

using hierarchy = std::vector<registration_factory>;
using order = std::vector<std::size_t>;

auto all_orders(std::size_t n) -> std::vector<order> {
    order permutation(n);
    std::iota(permutation.begin(), permutation.end(), std::size_t(0));
    std::vector<order> orders;

    do {
        orders.push_back(permutation);
    } while (std::next_permutation(permutation.begin(), permutation.end()));

    return orders;
}

// The listed order, its reverse, and `samples` shuffles. The shuffle is
// spelled out, rather than left to std::shuffle, so that every platform sees
// the same orders.
auto sampled_orders(std::size_t n, std::size_t samples) -> std::vector<order> {
    order listed(n);
    std::iota(listed.begin(), listed.end(), std::size_t(0));
    std::vector<order> orders{listed};
    orders.emplace_back(listed.rbegin(), listed.rend());
    std::mt19937 rng(20250510u);
    auto shuffled = listed;

    while (samples--) {
        for (auto i = shuffled.size(); i > 1; --i) {
            std::swap(shuffled[i - 1], shuffled[rng() % i]);
        }

        orders.push_back(shuffled);
    }

    return orders;
}

// Every v-table holds the slots of every class whose methods can be called
// on the class - the classes it is in the cone of - each at most once, and
// starts at the first of them and ends at the last.
template<class Compiler>
void check_slots(const Compiler& comp) {
    std::unordered_map<const class_*, std::vector<std::size_t>> held;

    for (const auto& cls : comp.classes) {
        for (const auto& mp : cls.used_by_vp) {
            for (auto derived : cls.transitive_derived) {
                held[derived].push_back(mp.method->slots[mp.param]);
            }
        }
    }

    for (const auto& cls : comp.classes) {
        auto& slots = held[&cls];
        std::sort(slots.begin(), slots.end());

        BOOST_TEST_CONTEXT("class " << &cls) {
            bool no_duplicate =
                std::adjacent_find(slots.begin(), slots.end()) == slots.end();
            BOOST_TEST(no_duplicate);

            if (slots.empty()) {
                BOOST_TEST(cls.vtbl.empty());
                BOOST_TEST(cls.first_slot == 0u);
            } else {
                BOOST_TEST(cls.first_slot == slots.front());
                BOOST_TEST(
                    cls.first_slot + cls.vtbl.size() == slots.back() + 1u);
            }
        }
    }
}

template<class Compiler>
auto total_vtbl_size(const Compiler& comp) -> std::size_t {
    std::size_t total = 0;

    for (const auto& cls : comp.classes) {
        total += cls.vtbl.size();
    }

    return total;
}

struct allocation_stats {
    std::size_t runs = 0;
    std::size_t min_total = std::numeric_limits<std::size_t>::max();
    std::size_t max_total = 0;
};

// Register `classes` in each of `orders`, initialize, check the invariants,
// let `fn` check what the test expects, and collect the total v-table size.
template<class Registry, class Fn>
auto allocate_in_orders(
    const hierarchy& classes, const std::vector<order>& orders, Fn fn)
    -> allocation_stats {
    allocation_stats stats;

    for (const auto& registration_order : orders) {
        BOOST_TEST_CONTEXT("registration order " << str(registration_order)) {
            std::vector<std::unique_ptr<registration>> live;

            for (auto i : registration_order) {
                live.push_back(classes[i]());
            }

            auto comp = initialize<Registry>();
            check_slots(comp);
            auto total = total_vtbl_size(comp);
            stats.min_total = std::min(stats.min_total, total);
            stats.max_total = std::max(stats.max_total, total);
            ++stats.runs;
            fn(comp);
        }
    }

    finalize<Registry>();

    BOOST_TEST_MESSAGE(
        boost::unit_test::framework::current_test_case().p_name
        << ": " << stats.runs << " orders, total v-table size "
        << stats.min_total << "-" << stats.max_total);

    return stats;
}

void expect_total(const allocation_stats& stats, std::size_t total) {
    BOOST_TEST(stats.min_total == total);
    BOOST_TEST(stats.max_total == total);
}

void expect_total_at_most(const allocation_stats& stats, std::size_t worst) {
    BOOST_TEST(stats.max_total <= worst);
}

// ----------------------------------------------------------------------------
// The slot chooser on its own.

BOOST_AUTO_TEST_CASE(test_pick_slot) {
    using boost::dynamic_bitset;
    using detail::generic_compiler;

    auto bits = [](std::initializer_list<std::size_t> slots) {
        dynamic_bitset<> result;

        for (auto slot : slots) {
            detail::set_bit(result, slot);
        }

        return result;
    };

    auto pick = [&](std::vector<dynamic_bitset<>> used) {
        std::vector<class_> classes(used.size());
        std::vector<class_*> cone;
        dynamic_bitset<> unavailable;

        for (std::size_t i = 0; i < used.size(); ++i) {
            classes[i].used_slots = used[i];
            cone.push_back(&classes[i]);
            detail::merge_into(used[i], unavailable);
        }

        return generic_compiler::pick_slot(cone, unavailable);
    };

    // Nothing allocated anywhere: slot 0, one entry per class.
    auto choice = pick({bits({}), bits({})});
    BOOST_TEST(choice.slot == 0u);
    BOOST_TEST(choice.cost == 2u);

    // A single range grows upward.
    choice = pick({bits({0, 1, 2})});
    BOOST_TEST(choice.slot == 3u);
    BOOST_TEST(choice.cost == 1u);

    // A hole in the range costs nothing.
    choice = pick({bits({0, 2})});
    BOOST_TEST(choice.slot == 1u);
    BOOST_TEST(choice.cost == 0u);

    // Up and down cost the same: the lowest slot wins.
    choice = pick({bits({1})});
    BOOST_TEST(choice.slot == 0u);
    BOOST_TEST(choice.cost == 1u);

    // Down is blocked by a derived class: up.
    choice = pick({bits({2}), bits({0, 1, 2})});
    BOOST_TEST(choice.slot == 3u);
    BOOST_TEST(choice.cost == 2u);

    // The derived classes' ranges count: down fills a hole in the derived
    // class, up grows it.
    choice = pick({bits({5}), bits({0, 1, 2, 3, 5})});
    BOOST_TEST(choice.slot == 4u);
    BOOST_TEST(choice.cost == 1u);

    // A class without a range lands where its derived classes grow least.
    choice = pick({bits({}), bits({0, 1}), bits({0, 1})});
    BOOST_TEST(choice.slot == 2u);
    BOOST_TEST(choice.cost == 3u);

    // ...and inside a hole they share, if there is one.
    choice = pick({bits({}), bits({0, 2}), bits({0, 2, 3})});
    BOOST_TEST(choice.slot == 1u);
    BOOST_TEST(choice.cost == 1u);
}

// ----------------------------------------------------------------------------
// Small lattices, in every registration order.

BOOST_AUTO_TEST_CASE(test_assign_slots_a_b1_c) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A
     / \
    B1  C
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct C : A {};

    ADD_METHOD(B);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<C, A, test_registry>,
    };

    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST_REQUIRE(check(comp[m_B])->slots.size() == 1u);
            BOOST_TEST(check(comp[m_B])->slots[0] == 0u);
            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 0u);
            BOOST_TEST(get_class<B>(comp)->vtbl.size() == 1u);
            BOOST_TEST(get_class<C>(comp)->vtbl.size() == 0u);
        });

    expect_total(stats, 1);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_b1_c1) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct C : A {};

    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<C, A, test_registry>,
    };

    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
            BOOST_TEST(check(comp[m_B])->slots[0] == 1u);
            BOOST_TEST(check(comp[m_C])->slots[0] == 1u);
            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);
            BOOST_TEST(get_class<B>(comp)->vtbl.size() == 2u);
            BOOST_TEST(get_class<C>(comp)->vtbl.size() == 2u);
        });

    expect_total(stats, 5);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_tree_any_order) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1
    |
    D1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct C : A {};
    struct D : B {};

    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);
    ADD_METHOD(D);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<C, A, test_registry>,
        registration_of<D, B, test_registry>,
    };

    // In a tree, every v-table is dense from slot 0, whatever the order.
    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
            BOOST_TEST(check(comp[m_B])->slots[0] == 1u);
            BOOST_TEST(check(comp[m_C])->slots[0] == 1u);
            BOOST_TEST(check(comp[m_D])->slots[0] == 2u);

            for (const auto& cls : comp.classes) {
                BOOST_TEST(cls.first_slot == 0u);
            }

            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);
            BOOST_TEST(get_class<B>(comp)->vtbl.size() == 2u);
            BOOST_TEST(get_class<C>(comp)->vtbl.size() == 2u);
            BOOST_TEST(get_class<D>(comp)->vtbl.size() == 3u);
        });

    expect_total(stats, 8);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_b1_d1_c1_d1) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1
     \ /
      D1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : virtual A {};
    struct C : virtual A {};
    struct D : B, C {};

    ADD_METHOD(A);
    USE_METHOD(B);
    USE_METHOD(C);
    ADD_METHOD(D);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<C, A, test_registry>,
        registration_of<D, B, C, test_registry>,
    };

    // B and C both sit above A's slot and must differ in D, so one of them has
    // a hole - there is no way around that. The old allocator gave the other
    // one a hole too, for a total of 11.
    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
            BOOST_TEST(check(comp[m_D])->slots[0] == 3u);
            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);
            BOOST_TEST(get_class<D>(comp)->vtbl.size() == 4u);
            auto b = get_class<B>(comp)->vtbl.size();
            auto c = get_class<C>(comp)->vtbl.size();
            BOOST_TEST(std::min(b, c) == 2u);
            BOOST_TEST(std::max(b, c) == 3u);
        });

    expect_total(stats, 10);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_b1_d1_c1_d1_e3) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1
     \ /  \
      D1  E3
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : virtual A {};
    struct C : virtual A {};
    struct E : C {};
    struct D : B, C {};

    ADD_METHOD(A);
    USE_METHOD(B);
    USE_METHOD(C);
    ADD_METHOD(D);
    USE_METHOD_N(E, 1);
    USE_METHOD_N(E, 2);
    USE_METHOD_N(E, 3);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<C, A, test_registry>,
        registration_of<E, C, test_registry>,
        registration_of<D, B, C, test_registry>,
    };

    // E fills the hole in C's v-table, if C has one. Old total: 16.
    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
            BOOST_TEST(check(comp[m_D])->slots[0] == 3u);
            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);
            BOOST_TEST(get_class<D>(comp)->vtbl.size() == 4u);
            BOOST_TEST(get_class<E>(comp)->vtbl.size() == 5u);
        });

    expect_total(stats, 15);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_c1_b1) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A1  B1
     \  /
      C1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B {
        virtual ~B() = default;
    };
    struct C : A, B {};

    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, test_registry>,
        registration_of<C, A, B, test_registry>,
    };

    // The second root to be allocated gets slot 1; its v-table starts there,
    // since leading unused slots are not stored.
    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            auto a = get_class<A>(comp);
            auto b = get_class<B>(comp);
            BOOST_TEST(
                check(comp[m_A])->slots[0] + check(comp[m_B])->slots[0] == 1u);
            BOOST_TEST(check(comp[m_C])->slots[0] == 2u);
            BOOST_TEST(a->vtbl.size() == 1u);
            BOOST_TEST(b->vtbl.size() == 1u);
            BOOST_TEST(a->first_slot + b->first_slot == 1u);
            BOOST_TEST(get_class<C>(comp)->vtbl.size() == 3u);
        });

    expect_total(stats, 5);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_hole_avoided) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A1  B1
     \ / \
     C1   E1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B {
        virtual ~B() = default;
    };
    struct C : A, B {};
    struct E : B {};

    USE_METHOD(A);
    USE_METHOD(B);
    USE_METHOD(C);
    USE_METHOD(E);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, test_registry>,
        registration_of<C, A, B, test_registry>,
        registration_of<E, B, test_registry>,
    };

    // E extends B's range by one entry. The old allocator, when it visited A,
    // C, B, E, put B at 2 and E at the lowest free slot, 0: a three-entry
    // v-table for E with a hole at 1, and a total of 8 in half of the orders.
    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(get_class<C>(comp)->vtbl.size() == 3u);
            BOOST_TEST(get_class<E>(comp)->vtbl.size() == 2u);
        });

    expect_total(stats, 7);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_grow_down) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A2      B1     W1
    |\ \   / \    / |
    Aa Ab C   E1  |
             \   |
              G--'     and V : A, W
    */

    struct A {
        virtual ~A() = default;
    };
    struct Aa : A {};
    struct Ab : A {};
    struct B {
        virtual ~B() = default;
    };
    struct W {
        virtual ~W() = default;
    };
    struct C : A, B {};
    struct V : A, W {};
    struct E : B {};
    struct G : E, W {};

    USE_METHOD_N(A, 1);
    USE_METHOD_N(A, 2);
    ADD_METHOD(B);
    ADD_METHOD(W);
    ADD_METHOD(E);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<Aa, A, test_registry>,
        registration_of<Ab, A, test_registry>,
        registration_of<B, test_registry>,
        registration_of<W, test_registry>,
        registration_of<C, A, B, test_registry>,
        registration_of<V, A, W, test_registry>,
        registration_of<E, B, test_registry>,
        registration_of<G, E, W, test_registry>,
    };

    // The cones decide the order: A (five classes) takes 0 and 1, B (four)
    // 2, W (three) 3 - G already holds 2 - and E, inheriting 2 from B, can
    // only extend downward, since G holds 3. The old allocator gave E the
    // lowest free slot, 0, with a hole at 1, in most orders: 20 to 23 in
    // total, against 20 in every order here.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 200), [&](const auto& comp) {
            BOOST_TEST(check(comp[m_B])->slots[0] == 2u);
            BOOST_TEST(check(comp[m_W])->slots[0] == 3u);
            BOOST_TEST(check(comp[m_E])->slots[0] == 1u);
            BOOST_TEST(get_class<E>(comp)->first_slot == 1u);
            BOOST_TEST(get_class<E>(comp)->vtbl.size() == 2u);
        });

    expect_total(stats, 20);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_cone_aware) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    Y5   P1   R4        Ya, Yb, Yc : Y
     \  / \  / \       Ra, Rb, Rc : R
      Z    X1   \
           \    |
            D---'
    */

    struct Y {
        virtual ~Y() = default;
    };
    struct Ya : Y {};
    struct Yb : Y {};
    struct Yc : Y {};
    struct P {
        virtual ~P() = default;
    };
    struct R {
        virtual ~R() = default;
    };
    struct Ra : R {};
    struct Rb : R {};
    struct Rc : R {};
    struct Z : Y, P {};
    struct X : P {};
    struct D : X, R {};

    USE_METHOD_N(Y, 1);
    USE_METHOD_N(Y, 2);
    USE_METHOD_N(Y, 3);
    USE_METHOD_N(Y, 4);
    USE_METHOD_N(Y, 5);
    ADD_METHOD(P);
    USE_METHOD_N(R, 1);
    USE_METHOD_N(R, 2);
    USE_METHOD_N(R, 3);
    USE_METHOD_N(R, 4);
    ADD_METHOD(X);

    hierarchy classes{
        registration_of<Y, test_registry>,
        registration_of<Ya, Y, test_registry>,
        registration_of<Yb, Y, test_registry>,
        registration_of<Yc, Y, test_registry>,
        registration_of<P, test_registry>,
        registration_of<R, test_registry>,
        registration_of<Ra, R, test_registry>,
        registration_of<Rb, R, test_registry>,
        registration_of<Rc, R, test_registry>,
        registration_of<Z, Y, P, test_registry>,
        registration_of<X, P, test_registry>,
        registration_of<D, X, R, test_registry>,
    };

    // Y and R, with the widest cones, take 0-4 and 0-3; P lands at 5, and X
    // inherits {5}. X's own range would grow by one either way, but D, which
    // holds 0-3 and 5, has a hole at 4 and would grow at 6: X goes down. The
    // old allocator: 51 to 55, depending on the order.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 200), [&](const auto& comp) {
            BOOST_TEST(check(comp[m_P])->slots[0] == 5u);
            BOOST_TEST(check(comp[m_X])->slots[0] == 4u);
            BOOST_TEST(get_class<X>(comp)->vtbl.size() == 2u);
            BOOST_TEST(get_class<D>(comp)->vtbl.size() == 6u);
        });

    expect_total(stats, 51);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_several_parameters_per_class) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A: two unary methods and a binary method on (A, A)
    |
    B1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};

    USE_METHOD_N(A, 1);
    USE_METHOD_N(A, 2);
    USE_METHOD2(A, A);
    ADD_METHOD(B);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
    };

    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 4u);
            BOOST_TEST(get_class<B>(comp)->vtbl.size() == 5u);
            BOOST_TEST(check(comp[m_B])->slots[0] == 4u);
        });

    expect_total(stats, 9);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_method_less_classes) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A1   R
    |    |
    B    S1
    |\   |
    C1 D1 T1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct C : B {};
    struct D : B {};
    struct R {
        virtual ~R() = default;
    };
    struct S : R {};
    struct T : S {};

    USE_METHOD(A);
    ADD_METHOD(C);
    ADD_METHOD(D);
    ADD_METHOD(S);
    ADD_METHOD(T);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<C, B, test_registry>,
        registration_of<D, B, test_registry>,
        registration_of<R, test_registry>,
        registration_of<S, R, test_registry>,
        registration_of<T, S, test_registry>,
    };

    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(get_class<B>(comp)->vtbl.size() == 1u);
            BOOST_TEST(check(comp[m_C])->slots[0] == 1u);
            BOOST_TEST(check(comp[m_D])->slots[0] == 1u);
            BOOST_TEST(get_class<R>(comp)->vtbl.size() == 0u);
            BOOST_TEST(check(comp[m_S])->slots[0] == 0u);
            BOOST_TEST(check(comp[m_T])->slots[0] == 1u);
        });

    expect_total(stats, 9);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_components) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A1  X1   and a binary method on (A, X)
    |   |
    B1  Y1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct X {
        virtual ~X() = default;
    };
    struct Y : X {};

    USE_METHOD(A);
    ADD_METHOD(B);
    USE_METHOD(X);
    ADD_METHOD(Y);
    USE_METHOD2(A, X);

    hierarchy classes{
        registration_of<A, test_registry>,
        registration_of<B, A, test_registry>,
        registration_of<X, test_registry>,
        registration_of<Y, X, test_registry>,
    };

    // Unrelated hierarchies do not see each other: both start at slot 0.
    auto stats = allocate_in_orders<test_registry>(
        classes, all_orders(classes.size()), [&](const auto& comp) {
            BOOST_TEST(get_class<A>(comp)->vtbl.size() == 2u);
            BOOST_TEST(get_class<B>(comp)->vtbl.size() == 3u);
            BOOST_TEST(get_class<X>(comp)->vtbl.size() == 2u);
            BOOST_TEST(get_class<Y>(comp)->vtbl.size() == 3u);
            BOOST_TEST(check(comp[m_B])->slots[0] == 2u);
            BOOST_TEST(check(comp[m_Y])->slots[0] == 2u);
        });

    expect_total(stats, 10);
}

// ----------------------------------------------------------------------------
// Real hierarchies, in a sample of registration orders.

// The stream classes of the C++ standard library, per the synopsis in
// [iostream.forward.overview]: basic_ios is a virtual base of basic_istream and
// basic_ostream, which basic_iostream joins back; the stream buffers are a
// separate tree.
namespace iso_cpp_streams {

struct ios_base {
    virtual ~ios_base() = default;
};
struct basic_ios : ios_base {};
struct basic_istream : virtual basic_ios {};
struct basic_ostream : virtual basic_ios {};
struct basic_iostream : basic_istream, basic_ostream {};
struct basic_ifstream : basic_istream {};
struct basic_ofstream : basic_ostream {};
struct basic_fstream : basic_iostream {};
struct basic_istringstream : basic_istream {};
struct basic_ostringstream : basic_ostream {};
struct basic_stringstream : basic_iostream {};
struct basic_ispanstream : basic_istream {};
struct basic_ospanstream : basic_ostream {};
struct basic_spanstream : basic_iostream {};
struct basic_osyncstream : basic_ostream {};

struct basic_streambuf {
    virtual ~basic_streambuf() = default;
};
struct basic_filebuf : basic_streambuf {};
struct basic_stringbuf : basic_streambuf {};
struct basic_spanbuf : basic_streambuf {};
struct basic_syncbuf : basic_streambuf {};

} // namespace iso_cpp_streams

BOOST_AUTO_TEST_CASE(test_assign_slots_iso_cpp_streams) {
    using test_registry = test_registry_<__COUNTER__>;
    using namespace iso_cpp_streams;

    USE_METHOD(ios_base);
    USE_METHOD(basic_ios);
    USE_METHOD(basic_istream);
    USE_METHOD(basic_ostream);
    USE_METHOD(basic_iostream);
    USE_METHOD(basic_ifstream);
    USE_METHOD(basic_ofstream);
    USE_METHOD(basic_fstream);
    USE_METHOD(basic_istringstream);
    USE_METHOD(basic_ostringstream);
    USE_METHOD(basic_stringstream);
    USE_METHOD(basic_ispanstream);
    USE_METHOD(basic_ospanstream);
    USE_METHOD(basic_spanstream);
    USE_METHOD(basic_osyncstream);
    USE_METHOD(basic_streambuf);
    USE_METHOD(basic_filebuf);
    USE_METHOD(basic_stringbuf);
    USE_METHOD(basic_spanbuf);
    USE_METHOD(basic_syncbuf);
    USE_METHOD2(basic_ostream, basic_streambuf);

    hierarchy classes{
        registration_of<ios_base, test_registry>,
        registration_of<basic_ios, ios_base, test_registry>,
        registration_of<basic_istream, basic_ios, test_registry>,
        registration_of<basic_ostream, basic_ios, test_registry>,
        registration_of<
            basic_iostream, basic_istream, basic_ostream, test_registry>,
        registration_of<basic_ifstream, basic_istream, test_registry>,
        registration_of<basic_ofstream, basic_ostream, test_registry>,
        registration_of<basic_fstream, basic_iostream, test_registry>,
        registration_of<basic_istringstream, basic_istream, test_registry>,
        registration_of<basic_ostringstream, basic_ostream, test_registry>,
        registration_of<basic_stringstream, basic_iostream, test_registry>,
        registration_of<basic_ispanstream, basic_istream, test_registry>,
        registration_of<basic_ospanstream, basic_ostream, test_registry>,
        registration_of<basic_spanstream, basic_iostream, test_registry>,
        registration_of<basic_osyncstream, basic_ostream, test_registry>,
        registration_of<basic_streambuf, test_registry>,
        registration_of<basic_filebuf, basic_streambuf, test_registry>,
        registration_of<basic_stringbuf, basic_streambuf, test_registry>,
        registration_of<basic_spanbuf, basic_streambuf, test_registry>,
        registration_of<basic_syncbuf, basic_streambuf, test_registry>,
    };

    // The old allocator: 95 to 97 over these orders.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 64), [](const auto&) {});

    expect_total(stats, 88);
}

// The servers and request handlers of CPython's socketserver module
// (Lib/socketserver.py, PSF License 2.0): two mix-ins combined with each of
// four servers, and a separate handler tree.
namespace socketserver {

struct BaseServer {
    virtual ~BaseServer() = default;
};
struct TCPServer : BaseServer {};
struct UDPServer : TCPServer {};
struct UnixStreamServer : TCPServer {};
struct UnixDatagramServer : UDPServer {};
struct ThreadingMixIn {
    virtual ~ThreadingMixIn() = default;
};
struct ForkingMixIn {
    virtual ~ForkingMixIn() = default;
};
struct ThreadingTCPServer : ThreadingMixIn, TCPServer {};
struct ThreadingUDPServer : ThreadingMixIn, UDPServer {};
struct ForkingTCPServer : ForkingMixIn, TCPServer {};
struct ForkingUDPServer : ForkingMixIn, UDPServer {};
struct ThreadingUnixStreamServer : ThreadingMixIn, UnixStreamServer {};
struct ThreadingUnixDatagramServer : ThreadingMixIn, UnixDatagramServer {};
struct ForkingUnixStreamServer : ForkingMixIn, UnixStreamServer {};
struct ForkingUnixDatagramServer : ForkingMixIn, UnixDatagramServer {};

struct BaseRequestHandler {
    virtual ~BaseRequestHandler() = default;
};
struct StreamRequestHandler : BaseRequestHandler {};
struct DatagramRequestHandler : BaseRequestHandler {};

} // namespace socketserver

BOOST_AUTO_TEST_CASE(test_assign_slots_socketserver) {
    using test_registry = test_registry_<__COUNTER__>;
    using namespace socketserver;

    USE_METHOD(BaseServer);
    USE_METHOD(TCPServer);
    USE_METHOD(UDPServer);
    USE_METHOD(UnixStreamServer);
    USE_METHOD(UnixDatagramServer);
    USE_METHOD(ThreadingMixIn);
    USE_METHOD(ForkingMixIn);
    USE_METHOD(ThreadingTCPServer);
    USE_METHOD(ThreadingUDPServer);
    USE_METHOD(ForkingTCPServer);
    USE_METHOD(ForkingUDPServer);
    USE_METHOD(ThreadingUnixStreamServer);
    USE_METHOD(ThreadingUnixDatagramServer);
    USE_METHOD(ForkingUnixStreamServer);
    USE_METHOD(ForkingUnixDatagramServer);
    USE_METHOD(BaseRequestHandler);
    USE_METHOD(StreamRequestHandler);
    USE_METHOD(DatagramRequestHandler);
    USE_METHOD2(BaseServer, BaseRequestHandler);

    hierarchy classes{
        registration_of<BaseServer, test_registry>,
        registration_of<TCPServer, BaseServer, test_registry>,
        registration_of<UDPServer, TCPServer, test_registry>,
        registration_of<UnixStreamServer, TCPServer, test_registry>,
        registration_of<UnixDatagramServer, UDPServer, test_registry>,
        registration_of<ThreadingMixIn, test_registry>,
        registration_of<ForkingMixIn, test_registry>,
        registration_of<
            ThreadingTCPServer, ThreadingMixIn, TCPServer, test_registry>,
        registration_of<
            ThreadingUDPServer, ThreadingMixIn, UDPServer, test_registry>,
        registration_of<
            ForkingTCPServer, ForkingMixIn, TCPServer, test_registry>,
        registration_of<
            ForkingUDPServer, ForkingMixIn, UDPServer, test_registry>,
        registration_of<
            ThreadingUnixStreamServer, ThreadingMixIn, UnixStreamServer,
            test_registry>,
        registration_of<
            ThreadingUnixDatagramServer, ThreadingMixIn, UnixDatagramServer,
            test_registry>,
        registration_of<
            ForkingUnixStreamServer, ForkingMixIn, UnixStreamServer,
            test_registry>,
        registration_of<
            ForkingUnixDatagramServer, ForkingMixIn, UnixDatagramServer,
            test_registry>,
        registration_of<BaseRequestHandler, test_registry>,
        registration_of<
            StreamRequestHandler, BaseRequestHandler, test_registry>,
        registration_of<
            DatagramRequestHandler, BaseRequestHandler, test_registry>,
    };

    // The old allocator: 76 to 84 over these orders. Its best order beats
    // this by one entry: the greedy choice is not always the optimum.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 64), [](const auto&) {});

    expect_total(stats, 77);
}

// Django's class-based generic views (django/views/generic/{base, detail,
// list, edit, dates}.py, BSD-3-Clause): mix-ins combined many times over.
// ContextMixin is reached twice by ModelFormMixin and BaseDeleteView, so it is
// a virtual base.
namespace django_views {

struct View {
    virtual ~View() = default;
};
struct ContextMixin {
    virtual ~ContextMixin() = default;
};
struct TemplateResponseMixin {
    virtual ~TemplateResponseMixin() = default;
};
struct TemplateView : TemplateResponseMixin, virtual ContextMixin, View {};
struct RedirectView : View {};

struct SingleObjectMixin : virtual ContextMixin {};
struct BaseDetailView : SingleObjectMixin, View {};
struct SingleObjectTemplateResponseMixin : TemplateResponseMixin {};
struct DetailView : SingleObjectTemplateResponseMixin, BaseDetailView {};

struct MultipleObjectMixin : virtual ContextMixin {};
struct BaseListView : MultipleObjectMixin, View {};
struct MultipleObjectTemplateResponseMixin : TemplateResponseMixin {};
struct ListView : MultipleObjectTemplateResponseMixin, BaseListView {};

struct FormMixin : virtual ContextMixin {};
struct ModelFormMixin : FormMixin, SingleObjectMixin {};
struct ProcessFormView : View {};
struct BaseFormView : FormMixin, ProcessFormView {};
struct FormView : TemplateResponseMixin, BaseFormView {};
struct BaseCreateView : ModelFormMixin, ProcessFormView {};
struct CreateView : SingleObjectTemplateResponseMixin, BaseCreateView {};
struct BaseUpdateView : ModelFormMixin, ProcessFormView {};
struct UpdateView : SingleObjectTemplateResponseMixin, BaseUpdateView {};
struct DeletionMixin {
    virtual ~DeletionMixin() = default;
};
struct BaseDeleteView : DeletionMixin, FormMixin, BaseDetailView {};
struct DeleteView : SingleObjectTemplateResponseMixin, BaseDeleteView {};

struct YearMixin {
    virtual ~YearMixin() = default;
};
struct MonthMixin {
    virtual ~MonthMixin() = default;
};
struct DayMixin {
    virtual ~DayMixin() = default;
};
struct WeekMixin {
    virtual ~WeekMixin() = default;
};
struct DateMixin {
    virtual ~DateMixin() = default;
};
struct BaseDateListView : MultipleObjectMixin, DateMixin, View {};
struct BaseArchiveIndexView : BaseDateListView {};
struct ArchiveIndexView :
    MultipleObjectTemplateResponseMixin,
    BaseArchiveIndexView {};
struct BaseYearArchiveView : YearMixin, BaseDateListView {};
struct YearArchiveView :
    MultipleObjectTemplateResponseMixin,
    BaseYearArchiveView {};
struct BaseMonthArchiveView : YearMixin, MonthMixin, BaseDateListView {};
struct MonthArchiveView :
    MultipleObjectTemplateResponseMixin,
    BaseMonthArchiveView {};
struct BaseWeekArchiveView : YearMixin, WeekMixin, BaseDateListView {};
struct WeekArchiveView :
    MultipleObjectTemplateResponseMixin,
    BaseWeekArchiveView {};
struct BaseDayArchiveView :
    YearMixin,
    MonthMixin,
    DayMixin,
    BaseDateListView {};
struct DayArchiveView :
    MultipleObjectTemplateResponseMixin,
    BaseDayArchiveView {};
struct BaseTodayArchiveView : BaseDayArchiveView {};
struct TodayArchiveView :
    MultipleObjectTemplateResponseMixin,
    BaseTodayArchiveView {};
struct BaseDateDetailView :
    YearMixin,
    MonthMixin,
    DayMixin,
    DateMixin,
    BaseDetailView {};
struct DateDetailView :
    SingleObjectTemplateResponseMixin,
    BaseDateDetailView {};

} // namespace django_views

BOOST_AUTO_TEST_CASE(test_assign_slots_django_views) {
    using test_registry = test_registry_<__COUNTER__>;
    using namespace django_views;

    USE_METHOD(View);
    USE_METHOD(ContextMixin);
    USE_METHOD(TemplateResponseMixin);
    USE_METHOD(TemplateView);
    USE_METHOD(RedirectView);
    USE_METHOD(SingleObjectMixin);
    USE_METHOD(BaseDetailView);
    USE_METHOD(SingleObjectTemplateResponseMixin);
    USE_METHOD(DetailView);
    USE_METHOD(MultipleObjectMixin);
    USE_METHOD(BaseListView);
    USE_METHOD(MultipleObjectTemplateResponseMixin);
    USE_METHOD(ListView);
    USE_METHOD(FormMixin);
    USE_METHOD(ModelFormMixin);
    USE_METHOD(ProcessFormView);
    USE_METHOD(BaseFormView);
    USE_METHOD(FormView);
    USE_METHOD(BaseCreateView);
    USE_METHOD(CreateView);
    USE_METHOD(BaseUpdateView);
    USE_METHOD(UpdateView);
    USE_METHOD(DeletionMixin);
    USE_METHOD(BaseDeleteView);
    USE_METHOD(DeleteView);
    USE_METHOD(YearMixin);
    USE_METHOD(MonthMixin);
    USE_METHOD(DayMixin);
    USE_METHOD(WeekMixin);
    USE_METHOD(DateMixin);
    USE_METHOD(BaseDateListView);
    USE_METHOD(BaseArchiveIndexView);
    USE_METHOD(ArchiveIndexView);
    USE_METHOD(BaseYearArchiveView);
    USE_METHOD(YearArchiveView);
    USE_METHOD(BaseMonthArchiveView);
    USE_METHOD(MonthArchiveView);
    USE_METHOD(BaseWeekArchiveView);
    USE_METHOD(WeekArchiveView);
    USE_METHOD(BaseDayArchiveView);
    USE_METHOD(DayArchiveView);
    USE_METHOD(BaseTodayArchiveView);
    USE_METHOD(TodayArchiveView);
    USE_METHOD(BaseDateDetailView);
    USE_METHOD(DateDetailView);

    hierarchy classes{
        registration_of<View, test_registry>,
        registration_of<ContextMixin, test_registry>,
        registration_of<TemplateResponseMixin, test_registry>,
        registration_of<
            TemplateView, TemplateResponseMixin, ContextMixin, View,
            test_registry>,
        registration_of<RedirectView, View, test_registry>,
        registration_of<SingleObjectMixin, ContextMixin, test_registry>,
        registration_of<BaseDetailView, SingleObjectMixin, View, test_registry>,
        registration_of<
            SingleObjectTemplateResponseMixin, TemplateResponseMixin,
            test_registry>,
        registration_of<
            DetailView, SingleObjectTemplateResponseMixin, BaseDetailView,
            test_registry>,
        registration_of<MultipleObjectMixin, ContextMixin, test_registry>,
        registration_of<BaseListView, MultipleObjectMixin, View, test_registry>,
        registration_of<
            MultipleObjectTemplateResponseMixin, TemplateResponseMixin,
            test_registry>,
        registration_of<
            ListView, MultipleObjectTemplateResponseMixin, BaseListView,
            test_registry>,
        registration_of<FormMixin, ContextMixin, test_registry>,
        registration_of<
            ModelFormMixin, FormMixin, SingleObjectMixin, test_registry>,
        registration_of<ProcessFormView, View, test_registry>,
        registration_of<
            BaseFormView, FormMixin, ProcessFormView, test_registry>,
        registration_of<
            FormView, TemplateResponseMixin, BaseFormView, test_registry>,
        registration_of<
            BaseCreateView, ModelFormMixin, ProcessFormView, test_registry>,
        registration_of<
            CreateView, SingleObjectTemplateResponseMixin, BaseCreateView,
            test_registry>,
        registration_of<
            BaseUpdateView, ModelFormMixin, ProcessFormView, test_registry>,
        registration_of<
            UpdateView, SingleObjectTemplateResponseMixin, BaseUpdateView,
            test_registry>,
        registration_of<DeletionMixin, test_registry>,
        registration_of<
            BaseDeleteView, DeletionMixin, FormMixin, BaseDetailView,
            test_registry>,
        registration_of<
            DeleteView, SingleObjectTemplateResponseMixin, BaseDeleteView,
            test_registry>,
        registration_of<YearMixin, test_registry>,
        registration_of<MonthMixin, test_registry>,
        registration_of<DayMixin, test_registry>,
        registration_of<WeekMixin, test_registry>,
        registration_of<DateMixin, test_registry>,
        registration_of<
            BaseDateListView, MultipleObjectMixin, DateMixin, View,
            test_registry>,
        registration_of<BaseArchiveIndexView, BaseDateListView, test_registry>,
        registration_of<
            ArchiveIndexView, MultipleObjectTemplateResponseMixin,
            BaseArchiveIndexView, test_registry>,
        registration_of<
            BaseYearArchiveView, YearMixin, BaseDateListView, test_registry>,
        registration_of<
            YearArchiveView, MultipleObjectTemplateResponseMixin,
            BaseYearArchiveView, test_registry>,
        registration_of<
            BaseMonthArchiveView, YearMixin, MonthMixin, BaseDateListView,
            test_registry>,
        registration_of<
            MonthArchiveView, MultipleObjectTemplateResponseMixin,
            BaseMonthArchiveView, test_registry>,
        registration_of<
            BaseWeekArchiveView, YearMixin, WeekMixin, BaseDateListView,
            test_registry>,
        registration_of<
            WeekArchiveView, MultipleObjectTemplateResponseMixin,
            BaseWeekArchiveView, test_registry>,
        registration_of<
            BaseDayArchiveView, YearMixin, MonthMixin, DayMixin,
            BaseDateListView, test_registry>,
        registration_of<
            DayArchiveView, MultipleObjectTemplateResponseMixin,
            BaseDayArchiveView, test_registry>,
        registration_of<
            BaseTodayArchiveView, BaseDayArchiveView, test_registry>,
        registration_of<
            TodayArchiveView, MultipleObjectTemplateResponseMixin,
            BaseTodayArchiveView, test_registry>,
        registration_of<
            BaseDateDetailView, YearMixin, MonthMixin, DayMixin, DateMixin,
            BaseDetailView, test_registry>,
        registration_of<
            DateDetailView, SingleObjectTemplateResponseMixin,
            BaseDateDetailView, test_registry>,
    };

    // The old allocator: 298 to 420 over these orders; its best order beats
    // this by five entries.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 64), [](const auto&) {});

    expect_total(stats, 303);
}

// The standardized condition types of ANSI Common Lisp (section 9.1.1, and
// the class precedence lists in the type dictionary): a wide tree with four
// multiply-derived types. `condition` and `error` are virtual bases, for
// `simple-error`, `simple-warning`, `simple-type-error` and `reader-error`.
namespace common_lisp {

struct condition {
    virtual ~condition() = default;
};
struct serious_condition : virtual condition {};
struct warning : virtual condition {};
struct simple_condition : virtual condition {};
struct error : serious_condition {};
struct storage_condition : serious_condition {};
struct style_warning : warning {};
struct simple_error : simple_condition, error {};
struct simple_warning : simple_condition, warning {};
struct type_error : error {};
struct simple_type_error : simple_condition, type_error {};
struct arithmetic_error : error {};
struct division_by_zero : arithmetic_error {};
struct floating_point_inexact : arithmetic_error {};
struct floating_point_invalid_operation : arithmetic_error {};
struct floating_point_overflow : arithmetic_error {};
struct floating_point_underflow : arithmetic_error {};
struct cell_error : error {};
struct unbound_variable : cell_error {};
struct unbound_slot : cell_error {};
struct undefined_function : cell_error {};
struct control_error : error {};
struct file_error : error {};
struct package_error : error {};
struct parse_error : virtual error {};
struct print_not_readable : error {};
struct program_error : error {};
struct stream_error : virtual error {};
struct end_of_file : stream_error {};
struct reader_error : parse_error, stream_error {};

} // namespace common_lisp

BOOST_AUTO_TEST_CASE(test_assign_slots_common_lisp_conditions) {
    using test_registry = test_registry_<__COUNTER__>;
    using namespace common_lisp;

    USE_METHOD(condition);
    USE_METHOD(serious_condition);
    USE_METHOD(warning);
    USE_METHOD(simple_condition);
    USE_METHOD(error);
    USE_METHOD(storage_condition);
    USE_METHOD(style_warning);
    USE_METHOD(simple_error);
    USE_METHOD(simple_warning);
    USE_METHOD(type_error);
    USE_METHOD(simple_type_error);
    USE_METHOD(arithmetic_error);
    USE_METHOD(division_by_zero);
    USE_METHOD(floating_point_inexact);
    USE_METHOD(floating_point_invalid_operation);
    USE_METHOD(floating_point_overflow);
    USE_METHOD(floating_point_underflow);
    USE_METHOD(cell_error);
    USE_METHOD(unbound_variable);
    USE_METHOD(unbound_slot);
    USE_METHOD(undefined_function);
    USE_METHOD(control_error);
    USE_METHOD(file_error);
    USE_METHOD(package_error);
    USE_METHOD(parse_error);
    USE_METHOD(print_not_readable);
    USE_METHOD(program_error);
    USE_METHOD(stream_error);
    USE_METHOD(end_of_file);
    USE_METHOD(reader_error);

    hierarchy classes{
        registration_of<condition, test_registry>,
        registration_of<serious_condition, condition, test_registry>,
        registration_of<warning, condition, test_registry>,
        registration_of<simple_condition, condition, test_registry>,
        registration_of<error, serious_condition, test_registry>,
        registration_of<storage_condition, serious_condition, test_registry>,
        registration_of<style_warning, warning, test_registry>,
        registration_of<simple_error, simple_condition, error, test_registry>,
        registration_of<
            simple_warning, simple_condition, warning, test_registry>,
        registration_of<type_error, error, test_registry>,
        registration_of<
            simple_type_error, simple_condition, type_error, test_registry>,
        registration_of<arithmetic_error, error, test_registry>,
        registration_of<division_by_zero, arithmetic_error, test_registry>,
        registration_of<
            floating_point_inexact, arithmetic_error, test_registry>,
        registration_of<
            floating_point_invalid_operation, arithmetic_error, test_registry>,
        registration_of<
            floating_point_overflow, arithmetic_error, test_registry>,
        registration_of<
            floating_point_underflow, arithmetic_error, test_registry>,
        registration_of<cell_error, error, test_registry>,
        registration_of<unbound_variable, cell_error, test_registry>,
        registration_of<unbound_slot, cell_error, test_registry>,
        registration_of<undefined_function, cell_error, test_registry>,
        registration_of<control_error, error, test_registry>,
        registration_of<file_error, error, test_registry>,
        registration_of<package_error, error, test_registry>,
        registration_of<parse_error, error, test_registry>,
        registration_of<print_not_readable, error, test_registry>,
        registration_of<program_error, error, test_registry>,
        registration_of<stream_error, error, test_registry>,
        registration_of<end_of_file, stream_error, test_registry>,
        registration_of<reader_error, parse_error, stream_error, test_registry>,
    };

    // The old allocator: 131 to 143 over these orders.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 64), [](const auto&) {});

    expect_total(stats, 126);
}

// The abstract base classes of CPython's collections.abc module (as listed in
// the library reference, PSF License 2.0). Iterable is reached twice by
// Sequence, and Sized by the mapping views, so both are virtual bases.
namespace collections_abc {

struct Container {
    virtual ~Container() = default;
};
struct Hashable {
    virtual ~Hashable() = default;
};
struct Iterable {
    virtual ~Iterable() = default;
};
struct Iterator : Iterable {};
struct Reversible : virtual Iterable {};
struct Generator : Iterator {};
struct Sized {
    virtual ~Sized() = default;
};
struct Callable {
    virtual ~Callable() = default;
};
struct Collection : virtual Sized, virtual Iterable, Container {};
struct Sequence : Reversible, Collection {};
struct MutableSequence : Sequence {};
struct ByteString : Sequence {};
struct Set : Collection {};
struct MutableSet : Set {};
struct Mapping : Collection {};
struct MutableMapping : Mapping {};
struct MappingView : virtual Sized {};
struct ItemsView : MappingView, Set {};
struct KeysView : MappingView, Set {};
struct ValuesView : MappingView, Collection {};
struct Awaitable {
    virtual ~Awaitable() = default;
};
struct Coroutine : Awaitable {};
struct AsyncIterable {
    virtual ~AsyncIterable() = default;
};
struct AsyncIterator : AsyncIterable {};
struct AsyncGenerator : AsyncIterator {};
struct Buffer {
    virtual ~Buffer() = default;
};

} // namespace collections_abc

BOOST_AUTO_TEST_CASE(test_assign_slots_collections_abc) {
    using test_registry = test_registry_<__COUNTER__>;
    using namespace collections_abc;

    USE_METHOD(Container);
    USE_METHOD(Hashable);
    USE_METHOD(Iterable);
    USE_METHOD(Iterator);
    USE_METHOD(Reversible);
    USE_METHOD(Generator);
    USE_METHOD(Sized);
    USE_METHOD(Callable);
    USE_METHOD(Collection);
    USE_METHOD(Sequence);
    USE_METHOD(MutableSequence);
    USE_METHOD(ByteString);
    USE_METHOD(Set);
    USE_METHOD(MutableSet);
    USE_METHOD(Mapping);
    USE_METHOD(MutableMapping);
    USE_METHOD(MappingView);
    USE_METHOD(ItemsView);
    USE_METHOD(KeysView);
    USE_METHOD(ValuesView);
    USE_METHOD(Awaitable);
    USE_METHOD(Coroutine);
    USE_METHOD(AsyncIterable);
    USE_METHOD(AsyncIterator);
    USE_METHOD(AsyncGenerator);
    USE_METHOD(Buffer);

    hierarchy classes{
        registration_of<Container, test_registry>,
        registration_of<Hashable, test_registry>,
        registration_of<Iterable, test_registry>,
        registration_of<Iterator, Iterable, test_registry>,
        registration_of<Reversible, Iterable, test_registry>,
        registration_of<Generator, Iterator, test_registry>,
        registration_of<Sized, test_registry>,
        registration_of<Callable, test_registry>,
        registration_of<Collection, Sized, Iterable, Container, test_registry>,
        registration_of<Sequence, Reversible, Collection, test_registry>,
        registration_of<MutableSequence, Sequence, test_registry>,
        registration_of<ByteString, Sequence, test_registry>,
        registration_of<Set, Collection, test_registry>,
        registration_of<MutableSet, Set, test_registry>,
        registration_of<Mapping, Collection, test_registry>,
        registration_of<MutableMapping, Mapping, test_registry>,
        registration_of<MappingView, Sized, test_registry>,
        registration_of<ItemsView, MappingView, Set, test_registry>,
        registration_of<KeysView, MappingView, Set, test_registry>,
        registration_of<ValuesView, MappingView, Collection, test_registry>,
        registration_of<Awaitable, test_registry>,
        registration_of<Coroutine, Awaitable, test_registry>,
        registration_of<AsyncIterable, test_registry>,
        registration_of<AsyncIterator, AsyncIterable, test_registry>,
        registration_of<AsyncGenerator, AsyncIterator, test_registry>,
        registration_of<Buffer, test_registry>,
    };

    // The old allocator: 102 to 114 over these orders.
    auto stats = allocate_in_orders<test_registry>(
        classes, sampled_orders(classes.size(), 64), [](const auto&) {});

    expect_total(stats, 96);
}

// ============================================================================
// Test finalize.

BOOST_AUTO_TEST_CASE(test_finalize_clears_vptr_vector) {
    using test_registry = test_registry_<__COUNTER__>;

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, test_registry>);
    (void)method<A, auto(virtual_<A&>)->void, test_registry>::fn;

    initialize<test_registry>();

    auto& vptrs = test_registry::state<policies::vptr_vector>().vptrs;
    BOOST_TEST(!vptrs.empty());

    // The vptr policy provides a finalize() (portable across MSVC/non-MSVC).
    static_assert(detail::has_finalize<
                  test_registry::policy<policies::vptr>, const std::tuple<>&>);

    finalize<test_registry>();
    BOOST_TEST(vptrs.empty()); // finalize cleared the vector
}

BOOST_AUTO_TEST_CASE(test_registries_do_not_share_vptr_state) {
    // Two distinct registries, both using vptr_vector, must each have their own
    // vptr_vector state; they do not share it.
    using registry1 = test_registry_<__COUNTER__>;
    using registry2 = test_registry_<__COUNTER__>;

    static_assert(!std::is_same_v<registry1, registry2>);

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, registry1>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, registry2>);
    (void)method<A, auto(virtual_<A&>)->void, registry1>::fn;
    (void)method<A, auto(virtual_<A&>)->void, registry2>::fn;

    initialize<registry1>();
    initialize<registry2>();

    auto& vptrs1 = registry1::state<policies::vptr_vector>().vptrs;
    auto& vptrs2 = registry2::state<policies::vptr_vector>().vptrs;

    // Distinct state objects.
    BOOST_TEST(
        static_cast<const void*>(&vptrs1) != static_cast<const void*>(&vptrs2));

    // Independent: clearing one registry's state leaves the other intact.
    BOOST_TEST(!vptrs1.empty());
    BOOST_TEST(!vptrs2.empty());
    finalize<registry1>();
    BOOST_TEST(vptrs1.empty());
    BOOST_TEST(!vptrs2.empty());
}
