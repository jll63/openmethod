// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// initialize() is transactional: if a policy's initialize throws, the
// registry keeps the dispatch state it had before the call - the static
// v-table pointers, the `next` pointers, the dispatch data and every policy's
// state - instead of pointers into a vector that unwinding has freed. It is
// marked as not initialized, though, until a call succeeds.

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>
#include <boost/openmethod/policies/vptr_map.hpp>

#define BOOST_TEST_MODULE initialize_transaction
#include <boost/test/unit_test.hpp>

#include "test_util.hpp"

#include <stdexcept>
#include <string>
#include <tuple>

using boost::mp11::mp_list;
using namespace boost::openmethod;

// A stateful policy whose `initialize` throws on demand. It writes to its
// state before throwing, so a rollback is observable there too; and it
// comes last in the policy list (see the static_assert below), so by the time
// it throws, the type_hash and vptr policies have written their new states.
struct explosive_policy {
    using category = explosive_policy;

    template<class Registry>
    struct fn {
        struct state {
            int generation = 0;
        };

        inline static bool armed = false;
        inline static int generations = 0;

        template<class Context, class... Options>
        static void initialize(const Context&, const std::tuple<Options...>&) {
            Registry::template state<explosive_policy>().generation =
                ++generations;

            if (armed) {
                throw std::runtime_error("boom");
            }
        }
    };
};

template<int N>
struct vector_registry :
    test_registry_<N>::template with<
        policies::runtime_checks, policies::throw_error_handler,
        explosive_policy> {};

template<int N>
struct map_registry :
    test_registry_<N>::template with<
        policies::runtime_checks, policies::throw_error_handler,
        policies::vptr_map<>, policies::indirect_vptr, explosive_policy> {};

template<int N>
using registries = mp_list<vector_registry<N>, map_registry<N>>;

template<class Registry>
constexpr bool explosive_comes_last =
    boost::mp11::mp_find<
        typename Registry::policy_list, explosive_policy>::value >
    boost::mp11::mp_find<
        typename Registry::policy_list,
        detail::find_first_derived_of<
            policies::vptr, typename Registry::policy_list>>::value;

static_assert(explosive_comes_last<vector_registry<0>>);
static_assert(explosive_comes_last<map_registry<0>>);

struct Animal {
    virtual ~Animal() = default;
};

struct Dog : Animal {};
struct Cat : Animal {};

struct BOOST_OPENMETHOD_ID(poke);

template<class Registry>
using poke = method<
    BOOST_OPENMETHOD_ID(poke), auto(virtual_<Animal&>)->std::string, Registry>;

template<class Registry>
auto poke_animal(Animal&) -> std::string {
    return "silence";
}

template<class Registry>
auto poke_dog(Dog& dog) -> std::string {
    return poke<Registry>::template next<poke_dog<Registry>>(dog) + " bark";
}

template<class Registry>
struct snapshot {
    using vptr_state =
        typename Registry::template policy<policies::vptr>::state;
    using type_hash = typename Registry::template policy<policies::type_hash>;

    snapshot() :
        dispatch_data(Registry::state().dispatch_data.data()),
        dog_vptr(Registry::template static_vptr<Dog>),
        cat_vptr(Registry::template static_vptr<Cat>),
        next(poke<Registry>::template next<poke_dog<Registry>>),
        hash_range(type_hash::hash_range()),
        policies(Registry::state().policies) {
    }

    auto vptrs() -> decltype(auto) {
        return (detail::get<vptr_state>(policies).vptrs);
    }

    const detail::word* dispatch_data;
    vptr_type dog_vptr;
    vptr_type cat_vptr;
    decltype(poke<Registry>::template next<poke_dog<Registry>>) next;
    std::pair<std::size_t, std::size_t> hash_range;
    decltype(Registry::state().policies) policies;
};

BOOST_AUTO_TEST_CASE_TEMPLATE(
    failed_reinitialize_keeps_previous_state, Registry,
    registries<__COUNTER__>) {
    using explosive = typename explosive_policy::template fn<
        typename Registry::registry_type>;
    using vptr_state = typename snapshot<Registry>::vptr_state;

    BOOST_OPENMETHOD_REGISTER(use_classes<Animal, Dog, Cat, Registry>);
    BOOST_OPENMETHOD_REGISTER(
        typename poke<Registry>::template override<poke_animal<Registry>>);
    BOOST_OPENMETHOD_REGISTER(
        typename poke<Registry>::template override<poke_dog<Registry>>);

    Dog dog;
    Cat cat;
    auto& st = Registry::state();

    initialize<Registry>();
    BOOST_TEST(st.initialized);
    BOOST_TEST(poke<Registry>::fn(dog) == "silence bark");
    BOOST_TEST(poke<Registry>::fn(cat) == "silence");
    BOOST_TEST(Registry::template state<explosive_policy>().generation == 1);

    snapshot<Registry> before;

    explosive::armed = true;
    BOOST_CHECK_THROW(initialize<Registry>(), std::runtime_error);
    explosive::armed = false;

    // Not initialized, but everything the previous call installed is still
    // there, and consistent...
    BOOST_TEST(!st.initialized);
    BOOST_TEST(st.dispatch_data.data() == before.dispatch_data);
    BOOST_TEST(Registry::template static_vptr<Dog> == before.dog_vptr);
    BOOST_TEST(Registry::template static_vptr<Cat> == before.cat_vptr);
    BOOST_TEST(
        poke<Registry>::template next<poke_dog<Registry>> == before.next);
    BOOST_TEST(
        (snapshot<Registry>::type_hash::hash_range() == before.hash_range));
    BOOST_TEST((detail::get<vptr_state>(st.policies).vptrs == before.vptrs()));
    // ...including the state of the policy that threw, after writing to it.
    BOOST_TEST(Registry::template state<explosive_policy>().generation == 1);

    // ...but dispatch is refused until an initialize() succeeds.
    BOOST_CHECK_THROW(poke<Registry>::fn(dog), not_initialized);

    initialize<Registry>();
    BOOST_TEST(st.initialized);
    BOOST_TEST(poke<Registry>::fn(dog) == "silence bark");
    BOOST_TEST(poke<Registry>::fn(cat) == "silence");
    BOOST_TEST(Registry::template state<explosive_policy>().generation == 3);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    failed_first_initialize_leaves_registry_clean, Registry,
    registries<__COUNTER__>) {
    using explosive = typename explosive_policy::template fn<
        typename Registry::registry_type>;
    using vptr_state = typename snapshot<Registry>::vptr_state;

    BOOST_OPENMETHOD_REGISTER(use_classes<Animal, Dog, Cat, Registry>);
    BOOST_OPENMETHOD_REGISTER(
        typename poke<Registry>::template override<poke_animal<Registry>>);
    BOOST_OPENMETHOD_REGISTER(
        typename poke<Registry>::template override<poke_dog<Registry>>);

    Dog dog;
    auto& st = Registry::state();

    explosive::armed = true;
    BOOST_CHECK_THROW(initialize<Registry>(), std::runtime_error);
    explosive::armed = false;

    BOOST_TEST(!st.initialized);
    BOOST_TEST(st.dispatch_data.empty());
    BOOST_TEST(Registry::template static_vptr<Dog> == nullptr);
    BOOST_TEST(detail::get<vptr_state>(st.policies).vptrs.empty());
    BOOST_TEST(Registry::template state<explosive_policy>().generation == 0);
    BOOST_CHECK_THROW(poke<Registry>::fn(dog), not_initialized);

    // finalize() has nothing to undo, and must not mind.
    finalize<Registry>();
    BOOST_TEST(!st.initialized);

    initialize<Registry>();
    BOOST_TEST(st.initialized);
    BOOST_TEST(poke<Registry>::fn(dog) == "silence bark");
}
