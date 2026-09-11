// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Drives `type_hash` policies directly, through a stand-in for the
// InitializeContext blueprint, rather than through `initialize()`. That isolates
// a policy from the rest of the compiler and - the point of the exercise - lets
// the type ids be *chosen*, so that a distribution which is hard to hash can be
// presented deliberately instead of being whatever this program's own classes
// happen to get.
//
// The type ids here are fabricated addresses. No `type_hash` policy
// dereferences a type id - each only casts it to an integer - but the trace
// option would, so this file must never pass one, and the registries it
// declares must never be handed to `boost::openmethod::initialize()`.

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/minimal_cover_hash.hpp>
#include <boost/openmethod/policies/minimal_perfect_hash.hpp>
#include <boost/openmethod/policies/two_level_hash.hpp>

#define BOOST_TEST_MODULE hash_policies
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

namespace bom = boost::openmethod;
namespace pol = boost::openmethod::policies;

namespace {

// A stand-in for InitializeContext. The policies under test use only
// classes_begin/classes_end, the type id range of each class, and has_option.
struct fake_class_view {
    const bom::type_id* first;
    const bom::type_id* last;

    auto type_id_begin() const {
        return first;
    }

    auto type_id_end() const {
        return last;
    }

    auto vptr() const -> bom::vptr_type {
        return nullptr;
    }

    auto static_vptr() const -> const bom::vptr_type* {
        return nullptr;
    }
};

struct fake_context {
    template<class Option>
    static constexpr bool has_option = false;

    std::vector<fake_class_view> views;

    auto classes_begin() const {
        return views.begin();
    }

    auto classes_end() const {
        return views.end();
    }
};

// One class per type id, which is what augment_classes() produces for a program
// whose classes are each registered once.
auto context_over(const std::vector<bom::type_id>& ids) -> fake_context {
    fake_context ctx;
    ctx.views.reserve(ids.size());

    for (const auto& id : ids) {
        ctx.views.push_back(fake_class_view{&id, &id + 1});
    }

    return ctx;
}

auto as_type_id(std::uint64_t value) -> bom::type_id {
    return reinterpret_cast<bom::type_id>(value);
}

// The four distributions that matter, all on the 16-byte grid the Itanium ABI
// guarantees for `type_info` records.
//
// `packed` is one module whose records happen to be adjacent. `diluted` is the
// realistic single-module case: v-tables are emitted between the records, so
// they are spread over many times their own size. `multi_module` is a program
// plus implicitly linked libraries. `dlopened` is the case this family of
// policies exists for - a program plus modules the loader placed wherever it
// liked, tens of terabytes apart.
//
// Each module gets a cursor that only ever moves forward, so the ids are
// distinct by construction. They have to be: a policy deduplicates the ids it
// is given, so a generator that repeats one would be testing the dedup rather
// than the hash, and would make an injectivity count come out short.
auto ids_over(std::size_t n, const std::uint64_t* bases, std::size_t modules)
    -> std::vector<bom::type_id> {
    std::vector<std::uint64_t> at(bases, bases + modules);
    std::vector<bom::type_id> ids;
    ids.reserve(n);

    for (std::size_t i = 0; i != n; ++i) {
        auto module = i % modules;
        ids.push_back(as_type_id(at[module]));
        at[module] += 16 * (1 + (i * 2654435761u) % 24);
    }

    return ids;
}

auto ids_packed(std::size_t n) -> std::vector<bom::type_id> {
    std::vector<bom::type_id> ids;
    ids.reserve(n);

    for (std::size_t i = 0; i != n; ++i) {
        ids.push_back(as_type_id(0x7f0000001000ull + i * 16));
    }

    return ids;
}

auto ids_diluted(std::size_t n) -> std::vector<bom::type_id> {
    const std::uint64_t base = 0x7f0000001000ull;

    return ids_over(n, &base, 1);
}

auto ids_multi_module(std::size_t n) -> std::vector<bom::type_id> {
    const std::uint64_t bases[] = {
        0x7f1000001000ull, 0x7f2940001000ull, 0x7fa13c001000ull,
        0x55d400001000ull};

    return ids_over(n, bases, 4);
}

auto ids_dlopened(std::size_t n) -> std::vector<bom::type_id> {
    // An executable low in the address space and three mappings the loader put
    // far above it: the span is tens of terabytes.
    const std::uint64_t bases[] = {
        0x000060bc53002000ull, 0x000075e940002000ull, 0x00007c9180002000ull,
        0x00007ffe12002000ull};

    return ids_over(n, bases, 4);
}

// What every one of these policies promises: `hash` is injective over the type
// ids it was initialized with, and `hash_range` brackets every value it returns.
template<class Registry, class Policy>
auto check_injective_over(const std::vector<bom::type_id>& ids) -> std::size_t {
    using fn = typename Policy::template fn<Registry>;

    auto ctx = context_over(ids);
    fn::initialize(ctx, std::tuple<>{});

    auto [low, high] = fn::hash_range();
    std::set<std::size_t> seen;

    for (auto id : ids) {
        auto index = fn::hash(id);
        BOOST_TEST(index >= low);
        BOOST_TEST(index <= high);
        BOOST_TEST(seen.insert(index).second);
    }

    BOOST_TEST(seen.size() == ids.size());
    fn::finalize(std::tuple<>{});

    return high - low + 1;
}

struct mph_registry :
    bom::registry<
        pol::std_rtti, pol::minimal_perfect_hash<>, pol::vptr_vector,
        pol::default_error_handler, pol::stderr_output> {};

struct mph_minimal_registry :
    bom::registry<
        pol::std_rtti, pol::minimal_perfect_hash<2, 100>, pol::vptr_vector,
        pol::default_error_handler, pol::stderr_output> {};

struct tlh_registry :
    bom::registry<
        pol::std_rtti, pol::two_level_hash<>, pol::vptr_vector,
        pol::default_error_handler, pol::stderr_output> {};

#if BOOST_OPENMETHOD_HAS_PEXT
struct mch_registry :
    bom::registry<
        pol::std_rtti, pol::minimal_cover_hash<>, pol::vptr_vector,
        pol::default_error_handler, pol::stderr_output> {};
#endif

} // namespace

// The fixture's own precondition: a generator that repeated an id would make
// every injectivity count below come out short, for no fault of the policies.
BOOST_AUTO_TEST_CASE(generators_produce_distinct_ids) {
    for (auto n : {std::size_t(1), std::size_t(17), std::size_t(1000)}) {
        for (auto&& named :
             {std::pair{"packed", ids_packed(n)},
              std::pair{"diluted", ids_diluted(n)},
              std::pair{"multi_module", ids_multi_module(n)},
              std::pair{"dlopened", ids_dlopened(n)}}) {
            BOOST_TEST_CONTEXT(named.first << ", n = " << n) {
                std::set<bom::type_id> distinct(
                    named.second.begin(), named.second.end());
                BOOST_TEST(distinct.size() == n);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(injective_on_every_distribution) {
    for (auto n :
         {std::size_t(1), std::size_t(2), std::size_t(17), std::size_t(256),
          std::size_t(1000)}) {
        for (auto&& named :
             {std::pair{"packed", ids_packed(n)},
              std::pair{"diluted", ids_diluted(n)},
              std::pair{"multi_module", ids_multi_module(n)},
              std::pair{"dlopened", ids_dlopened(n)}}) {
            BOOST_TEST_CONTEXT(named.first << ", n = " << n) {
                check_injective_over<mph_registry, pol::minimal_perfect_hash<>>(
                    named.second);
                check_injective_over<tlh_registry, pol::two_level_hash<>>(
                    named.second);
#if BOOST_OPENMETHOD_HAS_PEXT
                check_injective_over<mch_registry, pol::minimal_cover_hash<>>(
                    named.second);
#endif
            }
        }
    }
}

// The property that distinguishes this family: the table is sized by how many
// type ids there are, not by where they sit. The `dlopened` distribution spans
// tens of terabytes, and must cost exactly what the packed one costs.
BOOST_AUTO_TEST_CASE(table_size_is_independent_of_placement) {
    const std::size_t n = 1000;

    auto packed =
        check_injective_over<mph_registry, pol::minimal_perfect_hash<>>(
            ids_packed(n));
    auto spread =
        check_injective_over<mph_registry, pol::minimal_perfect_hash<>>(
            ids_dlopened(n));
    BOOST_TEST(packed == spread);

    auto packed_two = check_injective_over<tlh_registry, pol::two_level_hash<>>(
        ids_packed(n));
    auto spread_two = check_injective_over<tlh_registry, pol::two_level_hash<>>(
        ids_dlopened(n));
    BOOST_TEST(packed_two == spread_two);
}

// `LoadPercent = 100` asks for exactly one slot per type id.
BOOST_AUTO_TEST_CASE(minimal_perfect_hash_can_be_exactly_minimal) {
    for (auto n : {std::size_t(17), std::size_t(256), std::size_t(1000)}) {
        BOOST_TEST_CONTEXT("n = " << n) {
            auto slots = check_injective_over<
                mph_minimal_registry, pol::minimal_perfect_hash<2, 100>>(
                ids_diluted(n));
            BOOST_TEST(slots == n);
        }
    }
}

// The default leaves a little slack, and spends it: at most one slot in twenty
// more than there are type ids.
BOOST_AUTO_TEST_CASE(minimal_perfect_hash_is_near_minimal) {
    const std::size_t n = 1000;
    auto slots =
        check_injective_over<mph_registry, pol::minimal_perfect_hash<>>(
            ids_diluted(n));
    // `slots = ceil(n * 100 / LoadPercent)`, the policy's own formula.
    BOOST_TEST(slots == (n * 100 + 94) / 95);
}

// two_level_hash rounds up to a power of two, so between one and two slots per
// type id - and exactly one when the count is already a power of two.
BOOST_AUTO_TEST_CASE(two_level_hash_table_is_a_power_of_two) {
    for (auto n : {std::size_t(17), std::size_t(256), std::size_t(1000)}) {
        BOOST_TEST_CONTEXT("n = " << n) {
            auto slots =
                check_injective_over<tlh_registry, pol::two_level_hash<>>(
                    ids_diluted(n));
            BOOST_TEST((slots & (slots - 1)) == 0u);
            BOOST_TEST(slots >= n);
            BOOST_TEST(slots < n * 2);
        }
    }
}

// A type id may be registered by more than one module, so the same one can
// appear in several class views. The table is over the *distinct* ids.
BOOST_AUTO_TEST_CASE(repeated_type_ids_are_not_collisions) {
    auto ids = ids_diluted(64);
    auto doubled = ids;
    doubled.insert(doubled.end(), ids.begin(), ids.end());

    fake_context ctx;

    for (const auto& id : doubled) {
        ctx.views.push_back(fake_class_view{&id, &id + 1});
    }

    using fn = pol::minimal_perfect_hash<>::fn<mph_registry>;
    fn::initialize(ctx, std::tuple<>{});
    auto [low, high] = fn::hash_range();
    BOOST_TEST(high - low + 1 <= ids.size() + ids.size() / 20 + 1);

    std::set<std::size_t> seen;

    for (auto id : ids) {
        BOOST_TEST(seen.insert(fn::hash(id)).second);
    }

    fn::finalize(std::tuple<>{});
}

// finalize() releases what initialize() allocated. Re-initializing afterwards
// has to work, which is what `initialize()` does on every call.
BOOST_AUTO_TEST_CASE(initialize_after_finalize) {
    using fn = pol::minimal_perfect_hash<>::fn<mph_registry>;

    for (int round = 0; round != 3; ++round) {
        auto ids = ids_diluted(128 + std::size_t(round) * 8);
        auto ctx = context_over(ids);
        fn::initialize(ctx, std::tuple<>{});
        BOOST_TEST(fn::hash_range().second + 1 >= ids.size());
        fn::finalize(std::tuple<>{});
        BOOST_TEST(fn::hash_range().second == 0u);
    }
}
