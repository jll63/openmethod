// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.

#ifndef BOOST_OPENMETHOD_RESEARCH_PEXT_INDEX_HPP
#define BOOST_OPENMETHOD_RESEARCH_PEXT_INDEX_HPP

#include <boost/openmethod/policies/fast_perfect_hash.hpp>
#include <boost/openmethod/preamble.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#if !defined(__BMI2__)
#error "pext_index needs BMI2: compile with -mbmi2 (or -march=native)"
#endif

#include <immintrin.h>

namespace boost::openmethod::research {

//! Map type ids to indexes by extracting a minimal cover of their bits.
//!
//! A @ref type_hash policy in the form `H(x) = pext(x, mask)`: the bits of
//! `x` selected by `mask`, packed into the low `popcount(mask)` positions
//! (BMI2 parallel bit extract, one instruction). The index range is
//! `[0, 2^popcount(mask))`.
//!
//! `mask` is a *minimal cover*: a smallest-found set of bit positions such
//! that `x & mask` is still injective over the registered type ids. That is
//! exactly the condition for `pext` to be injective, so the search never needs
//! `pext` itself. It is found deterministically, without the randomized
//! multiplier search of @ref policies::fast_perfect_hash:
//!
//! @li the bits that vary at all are trivially a cover;
//! @li a greedy pass drops bits, lowest entropy first, while injectivity
//!   holds;
//! @li a second greedy pass builds a cover bottom-up, adding the bit that
//!   resolves the most collisions each time, then trims it the same way;
//! @li the smaller of the two wins.
//!
//! What this buys over `(x - base) / d` (see @ref direct_type_id_index) is
//! immunity to *where* the modules are. Type ids from different modules
//! differ in many high bits, but those bits are perfectly correlated - they
//! all encode "which module" - so the cover keeps about `log2(modules)` of
//! them and the rest cost nothing. The span no longer matters; only the
//! within-module layout does.
//!
//! Inspired by "Perfect Hashing in an Imperfect World", Joaquín M. López
//! Muñoz.
//!
//! @tparam MaxBits Refuse a cover wider than this (the table is `8 << bits`).
template<std::size_t MaxBits = 24>
struct pext_index : policies::type_hash {

    //! The minimal cover found is too wide for a table.
    struct too_many_bits : openmethod_error {
        //! Number of registered type ids.
        std::size_t classes;
        //! Width of the smallest cover found.
        std::size_t bits;

        template<class Registry, class Stream>
        auto write(Stream& os) const -> void;
    };

    using errors = std::variant<too_many_bits>;

    //! `state` layout when runtime checks are disabled.
    struct no_checks {
        //! The bit positions to extract.
        std::uint64_t mask;
        //! The highest index in use.
        std::size_t max_value;
    };

    //! `state` layout when runtime checks are enabled: adds the table of
    //! registered type ids used to validate hashed types.
    struct with_checks : no_checks {
        std::vector<type_id> control;
    };

    //! A TypeHashFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy
    template<class Registry>
    class fn {
      public:
        using state = std::conditional_t<
            Registry::has_runtime_checks, with_checks, no_checks>;

      private:
        static auto& st() {
            return Registry::template state<pext_index>();
        }

        static void check(std::size_t index, type_id type);

        static auto injective(
            const std::vector<std::uint64_t>& ids, std::uint64_t mask,
            std::vector<std::uint64_t>& scratch) -> bool;
        static auto collisions(
            const std::vector<std::uint64_t>& ids, std::uint64_t mask,
            std::vector<std::uint64_t>& scratch) -> std::size_t;
        static auto minimal_cover(const std::vector<std::uint64_t>& ids)
            -> std::uint64_t;

      public:
        //! Finds the cover.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @param options A tuple of option objects.
        template<class Context, class... Options>
        static auto initialize(
            const Context& ctx, const std::tuple<Options...>& options) -> void;

        //! Returns the hash range: `[0, max index in use]`.
        static auto hash_range() -> std::pair<std::size_t, std::size_t> {
            return std::pair{std::size_t(0), st().max_value};
        }

        //! Map a type id to an index
        //!
        //! @param type The type_id to map
        //! @return The index
        BOOST_FORCEINLINE
        static auto hash(type_id type) -> std::size_t {
            auto index = std::size_t(_pext_u64(
                static_cast<std::uint64_t>(
                    reinterpret_cast<detail::uintptr>(type)),
                st().mask));

            if constexpr (Registry::has_runtime_checks) {
                check(index, type);
            }

            return index;
        }

        //! Releases the control table.
        template<class... Options>
        static auto finalize(const std::tuple<Options...>& options) -> void {
            (void)options;

            if constexpr (Registry::has_runtime_checks) {
                st().control.clear();
                st().control.shrink_to_fit();
            }
        }
    };
};

template<std::size_t MaxBits>
template<class Registry>
auto pext_index<MaxBits>::fn<Registry>::injective(
    const std::vector<std::uint64_t>& ids, std::uint64_t mask,
    std::vector<std::uint64_t>& scratch) -> bool {
    scratch.clear();

    for (auto id : ids) {
        scratch.push_back(id & mask);
    }

    std::sort(scratch.begin(), scratch.end());

    return std::adjacent_find(scratch.begin(), scratch.end()) == scratch.end();
}

template<std::size_t MaxBits>
template<class Registry>
auto pext_index<MaxBits>::fn<Registry>::collisions(
    const std::vector<std::uint64_t>& ids, std::uint64_t mask,
    std::vector<std::uint64_t>& scratch) -> std::size_t {
    scratch.clear();

    for (auto id : ids) {
        scratch.push_back(id & mask);
    }

    std::sort(scratch.begin(), scratch.end());
    std::size_t count = 0;

    for (std::size_t i = 1; i < scratch.size(); ++i) {
        count += scratch[i] == scratch[i - 1];
    }

    return count;
}

template<std::size_t MaxBits>
template<class Registry>
auto pext_index<MaxBits>::fn<Registry>::minimal_cover(
    const std::vector<std::uint64_t>& ids) -> std::uint64_t {
    // The bits that vary at all: trivially a cover, since two distinct ids
    // differ in at least one of them.
    std::uint64_t universe = 0;

    for (auto id : ids) {
        universe |= id ^ ids.front();
    }

    // Entropy of each varying bit: a bit almost every id agrees on separates
    // few pairs, so it is the first candidate for dropping.
    struct bit_info {
        int bit;
        double entropy;
    };

    std::vector<bit_info> bits;

    for (int bit = 0; bit < 64; ++bit) {
        if (!((universe >> bit) & 1)) {
            continue;
        }

        std::size_t ones = 0;

        for (auto id : ids) {
            ones += (id >> bit) & 1;
        }

        auto p = double(ones) / double(ids.size());
        auto entropy = (p <= 0.0 || p >= 1.0)
            ? 0.0
            : -(p * std::log2(p) + (1 - p) * std::log2(1 - p));
        bits.push_back({bit, entropy});
    }

    std::stable_sort(
        bits.begin(), bits.end(), [](const bit_info& a, const bit_info& b) {
            return a.entropy < b.entropy;
        });

    std::vector<std::uint64_t> scratch;
    scratch.reserve(ids.size());

    auto drop = [&](std::uint64_t mask) {
        for (const auto& info : bits) {
            auto candidate = mask & ~(std::uint64_t(1) << info.bit);

            if (candidate != mask && injective(ids, candidate, scratch)) {
                mask = candidate;
            }
        }

        return mask;
    };

    auto by_drop = drop(universe);

    // Bottom-up: add the bit that resolves the most collisions.
    std::uint64_t by_add = 0;

    while (!injective(ids, by_add, scratch)) {
        int best_bit = -1;
        std::size_t best_count = (std::numeric_limits<std::size_t>::max)();

        for (const auto& info : bits) {
            auto bit = std::uint64_t(1) << info.bit;

            if (by_add & bit) {
                continue;
            }

            auto count = collisions(ids, by_add | bit, scratch);

            if (count < best_count) {
                best_count = count;
                best_bit = info.bit;
            }
        }

        by_add |= std::uint64_t(1) << best_bit;
    }

    by_add = drop(by_add);

    return __builtin_popcountll(by_add) < __builtin_popcountll(by_drop)
        ? by_add
        : by_drop;
}

template<std::size_t MaxBits>
template<class Registry>
template<class Context, class... Options>
auto pext_index<MaxBits>::fn<Registry>::initialize(
    const Context& ctx, const std::tuple<Options...>& options) -> void {
    (void)options;

    std::vector<std::uint64_t> ids;

    for (auto iter = ctx.classes_begin(); iter != ctx.classes_end(); ++iter) {
        for (auto type_iter = iter->type_id_begin();
             type_iter != iter->type_id_end(); ++type_iter) {
            ids.push_back(
                static_cast<std::uint64_t>(
                    reinterpret_cast<detail::uintptr>(*type_iter)));
        }
    }

    // One class may be registered under the same type id by several modules;
    // the cover is over *distinct* ids.
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    if (ids.empty()) {
        st().mask = 0;
        st().max_value = 0;

        return;
    }

    auto mask = ids.size() == 1 ? 0 : minimal_cover(ids);
    auto bits = std::size_t(__builtin_popcountll(mask));

    if (bits > MaxBits) {
        too_many_bits error;
        error.classes = ids.size();
        error.bits = bits;

        if constexpr (Registry::has_error_handler) {
            Registry::error_handler::error(error);
        }

        abort();
    }

    st().mask = mask;
    st().max_value = 0;

    for (auto id : ids) {
        st().max_value =
            (std::max)(st().max_value, std::size_t(_pext_u64(id, mask)));
    }

    if constexpr (Context::template has_option<trace>) {
        ctx.tr << "  type ids: " << ids.size() << ", cover: " << bits
               << " bits, table: " << (st().max_value + 1) << " slots\n";
    }

    if constexpr (Registry::has_runtime_checks) {
        st().control.assign(st().max_value + 1, type_id(detail::uintptr_max));

        for (auto id : ids) {
            st().control[std::size_t(_pext_u64(id, mask))] =
                reinterpret_cast<type_id>(id);
        }
    }
}

template<std::size_t MaxBits>
template<class Registry>
void pext_index<MaxBits>::fn<Registry>::check(std::size_t index, type_id type) {
    if (index > st().max_value || st().control[index] != type) {
        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

template<std::size_t MaxBits>
template<class Registry, class Stream>
auto pext_index<MaxBits>::too_many_bits::write(Stream& os) const -> void {
    os << "the smallest bit cover of " << classes << " type ids is " << bits
       << " bits wide, more than the " << MaxBits << " allowed\n";
}

} // namespace boost::openmethod::research

#endif
