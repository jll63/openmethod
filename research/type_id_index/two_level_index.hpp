// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.

#ifndef BOOST_OPENMETHOD_RESEARCH_TWO_LEVEL_INDEX_HPP
#define BOOST_OPENMETHOD_RESEARCH_TWO_LEVEL_INDEX_HPP

#include <boost/openmethod/policies/fast_perfect_hash.hpp> // detail::uintptr
#include <boost/openmethod/preamble.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace boost::openmethod::research {

//! Map type ids to an index with two multiply-shifts.
//!
//! A @ref type_hash policy in the form
//!
//! @code
//! h     = m1 * x;                  // mix once
//! h1(x) = h >> s1;                 // which bucket, 2^b of them
//! h2(x) = (m2[h1(x)] * h) >> s2;   // the index, into 2^t slots
//! @endcode
//!
//! The first level is one imperfect multiply-shift into a fixed number of
//! buckets; the second is a *per-bucket* multiplier, found by trial, that
//! sends every id in its bucket to a slot that is still free.
//!
//! The second multiply has to be applied to `h`, not to `x` itself. Type ids
//! within a module are 16 to 48 bytes apart, so for two of them `m2 * x`
//! differs by `m2 * 16` or so - below `2^(64 - t)` for any 32-bit `m2`, which
//! means the shift discards the difference and the two ids collide under
//! every multiplier the search can try. Measured, the table had to grow to
//! 2^24 slots for 1000 type ids before the search succeeded. Multiplying the
//! mixed `h` instead costs nothing: `h` is ready three cycles in, long before
//! `m2` arrives from the directory.
//!
//! This is the same displacement idea as @ref hd_index, and the two differ
//! in exactly one place: what turns the displaced key into an index. This one
//! shifts, so the table must have a power-of-two number of slots; `hd_index`
//! reduces with the top half of a product, so its table can be any size. The
//! shift is a cycle or three cheaper on the dispatch path and the table is
//! anything from 1.0 to 2.0 times the type id count instead of a flat 1.05.
//!
//! Note what it is *not*: the classical two-level construction of Fredman,
//! Komlós and Szemerédi gives each bucket a private sub-table and adds a
//! per-bucket base, `base[h1] + h2(x)`. That wastes the slack inside every
//! sub-table and needs a second field in the directory. Here the buckets all
//! index one shared table and interleave in it - a bucket placed late fills
//! the holes the earlier ones left - which is what keeps the load factor near
//! one.
//!
//! `m1` is not taken as given. `M1Candidates` multipliers are tried and the
//! one whose buckets are most even is kept, scored by the sum of the squared
//! bucket sizes. Evening the buckets shortens the search, because the cost of
//! placing a bucket grows steeply with its size.
//!
//! @tparam Lambda Average bucket size.
//! @tparam M1Candidates How many first-level multipliers to score.
//! @tparam MaxTableBits Give up once the table would exceed `2^MaxTableBits`.
template<
    std::size_t Lambda = 4, std::size_t M1Candidates = 8,
    std::size_t MaxTableBits = 26>
struct two_level_index : policies::type_hash {

    //! No table up to `2^MaxTableBits` admitted a complete assignment.
    struct search_error : openmethod_error {
        //! Number of registered type ids.
        std::size_t classes;
        //! Widest table tried.
        std::size_t table_bits;

        template<class Registry, class Stream>
        auto write(Stream& os) const -> void;
    };

    using errors = std::variant<search_error>;

    static_assert(Lambda > 0);
    static_assert(M1Candidates > 0);

    //! `state` layout when runtime checks are disabled.
    struct no_checks {
        //! First-level multiplier.
        std::uint64_t m1;
        //! `64 - b`.
        std::size_t s1;
        //! `64 - t`.
        std::size_t s2;
        //! Second-level multiplier, one per bucket.
        std::vector<std::uint32_t> m2;
        //! `2^t`.
        std::size_t slots;
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
            return Registry::template state<two_level_index>();
        }

        static void check(std::size_t index, type_id type);

        // The second-level multiplier tried at step `k`. Odd, because an even
        // multiplier throws away the key's top bits.
        static auto m2_at(std::uint32_t k) -> std::uint32_t {
            auto z = k * 0x9e3779b9u;
            z ^= z >> 15;
            z *= 0x85ebca6bu;
            z ^= z >> 13;

            return z | 1u;
        }

        static auto m1_at(std::uint64_t z) -> std::uint64_t {
            z += 0x9e3779b97f4a7c15ull;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;

            return (z ^ (z >> 31)) | 1;
        }

        // Sum of the squared bucket sizes: the quantity the search cost
        // tracks, and what picking m1 minimizes.
        static auto spread(
            const std::vector<std::uint64_t>& ids, std::uint64_t m1,
            std::size_t s1, std::size_t buckets) -> std::size_t;

        static auto build(
            const std::vector<std::uint64_t>& ids, std::uint64_t m1,
            std::size_t s1, std::size_t s2, std::size_t slots,
            std::vector<std::uint32_t>& m2) -> bool;

      public:
        //! Finds `m1` and the per-bucket multipliers.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @param options A tuple of option objects.
        template<class Context, class... Options>
        static auto initialize(
            const Context& ctx, const std::tuple<Options...>& options) -> void;

        //! Bytes of second-level multipliers, over and above the `8 * slots`
        //! that `vptr_vector` allocates.
        static auto aux_bytes() -> std::size_t {
            return st().m2.size() * sizeof(std::uint32_t);
        }

        //! Returns the hash range: `[0, slots - 1]`.
        static auto hash_range() -> std::pair<std::size_t, std::size_t> {
            return std::pair{std::size_t(0), st().slots ? st().slots - 1 : 0};
        }

        //! Map a type id to an index
        //!
        //! @param type The type_id to map
        //! @return The index
        BOOST_FORCEINLINE
        static auto hash(type_id type) -> std::size_t {
            auto h = std::uint64_t(reinterpret_cast<detail::uintptr>(type)) *
                st().m1;
            auto bucket = std::size_t(h >> st().s1);
            auto index =
                std::size_t((std::uint64_t(st().m2[bucket]) * h) >> st().s2);

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

template<std::size_t Lambda, std::size_t M1Candidates, std::size_t MaxTableBits>
template<class Registry>
auto two_level_index<Lambda, M1Candidates, MaxTableBits>::fn<Registry>::spread(
    const std::vector<std::uint64_t>& ids, std::uint64_t m1, std::size_t s1,
    std::size_t buckets) -> std::size_t {
    std::vector<std::uint32_t> count(buckets, 0);

    for (auto id : ids) {
        ++count[std::size_t((m1 * id) >> s1)];
    }

    std::size_t total = 0;

    for (auto c : count) {
        total += std::size_t(c) * c;
    }

    return total;
}

template<std::size_t Lambda, std::size_t M1Candidates, std::size_t MaxTableBits>
template<class Registry>
auto two_level_index<Lambda, M1Candidates, MaxTableBits>::fn<Registry>::build(
    const std::vector<std::uint64_t>& ids, std::uint64_t m1, std::size_t s1,
    std::size_t s2, std::size_t slots, std::vector<std::uint32_t>& m2) -> bool {
    auto n = ids.size();
    auto buckets = m2.size();

    // Group by bucket: counting sort into a CSR-style pair of arrays.
    std::vector<std::uint64_t> keys(n);
    std::vector<std::uint32_t> bucket_of(n);

    for (std::size_t i = 0; i != n; ++i) {
        keys[i] = m1 * ids[i];
        bucket_of[i] = std::uint32_t(keys[i] >> s1);
    }

    std::vector<std::uint32_t> start(buckets + 1, 0);

    for (auto b : bucket_of) {
        ++start[b + 1];
    }

    std::partial_sum(start.begin(), start.end(), start.begin());
    std::vector<std::uint32_t> members(n);
    auto fill = start;

    for (std::size_t i = 0; i != n; ++i) {
        members[fill[bucket_of[i]]++] = std::uint32_t(i);
    }

    // Largest buckets first: a big bucket is placeable only while the table
    // is still mostly empty.
    std::vector<std::uint32_t> order(buckets);
    std::iota(order.begin(), order.end(), std::uint32_t(0));
    std::stable_sort(
        order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
            return (start[a + 1] - start[a]) > (start[b + 1] - start[b]);
        });

    const std::uint32_t max_tries = 1u << 20;
    std::vector<char> occupied(slots, 0);
    std::vector<std::size_t> placed;
    placed.reserve(64);

    for (auto b : order) {
        auto first = start[b], last = start[b + 1];

        if (first == last) {
            m2[b] = 1;

            continue;
        }

        bool done = false;

        for (std::uint32_t k = 0; k != max_tries; ++k) {
            auto candidate = m2_at(k);
            placed.clear();
            bool ok = true;

            for (auto m = first; m != last; ++m) {
                auto index = std::size_t(
                    (std::uint64_t(candidate) * keys[members[m]]) >> s2);

                if (occupied[index] ||
                    std::find(placed.begin(), placed.end(), index) !=
                        placed.end()) {
                    ok = false;

                    break;
                }

                placed.push_back(index);
            }

            if (ok) {
                for (auto index : placed) {
                    occupied[index] = 1;
                }

                m2[b] = candidate;
                done = true;

                break;
            }
        }

        if (!done) {
            return false;
        }
    }

    return true;
}

template<std::size_t Lambda, std::size_t M1Candidates, std::size_t MaxTableBits>
template<class Registry>
template<class Context, class... Options>
auto two_level_index<Lambda, M1Candidates, MaxTableBits>::fn<Registry>::
    initialize(const Context& ctx, const std::tuple<Options...>& options)
        -> void {
    (void)options;

    std::vector<std::uint64_t> ids;

    for (auto iter = ctx.classes_begin(); iter != ctx.classes_end(); ++iter) {
        for (auto type_iter = iter->type_id_begin();
             type_iter != iter->type_id_end(); ++type_iter) {
            ids.push_back(
                std::uint64_t(reinterpret_cast<detail::uintptr>(*type_iter)));
        }
    }

    // One class may be registered under the same type id by several modules;
    // the table is over *distinct* ids.
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    auto n = ids.size();

    if (n == 0) {
        st().m1 = 1;
        st().s1 = 63;
        st().s2 = 63;
        st().m2.assign(2, 1);
        st().slots = 0;

        return;
    }

    // Never fewer than two buckets, so the shift stays below 64.
    std::size_t buckets = 2, b = 1;

    while (buckets * Lambda < n) {
        buckets <<= 1;
        ++b;
    }

    // Score M1Candidates first-level multipliers and keep the most even.
    auto m1 = m1_at(0);
    auto best = spread(ids, m1, 64 - b, buckets);

    for (std::size_t i = 1; i != M1Candidates; ++i) {
        auto candidate = m1_at(i);
        auto score = spread(ids, candidate, 64 - b, buckets);

        if (score < best) {
            best = score;
            m1 = candidate;
        }
    }

    // Smallest power-of-two table that can hold them, grown on failure.
    std::size_t t = 1;

    while ((std::size_t(1) << t) < n) {
        ++t;
    }

    std::vector<std::uint32_t> m2(buckets);
    bool found = false;

    for (; t <= MaxTableBits; ++t) {
        if (build(ids, m1, 64 - b, 64 - t, std::size_t(1) << t, m2)) {
            found = true;

            break;
        }
    }

    if (!found) {
        search_error error;
        error.classes = n;
        error.table_bits = MaxTableBits;

        if constexpr (Registry::has_error_handler) {
            Registry::error_handler::error(error);
        }

        abort();
    }

    st().m1 = m1;
    st().s1 = 64 - b;
    st().s2 = 64 - t;
    st().slots = std::size_t(1) << t;
    st().m2 = std::move(m2);

    if constexpr (Context::template has_option<trace>) {
        ctx.tr << "  type ids: " << n << ", buckets: " << buckets
               << ", table: " << st().slots << " slots\n";
    }

    if constexpr (Registry::has_runtime_checks) {
        st().control.assign(st().slots, type_id(detail::uintptr_max));

        for (auto id : ids) {
            auto h = st().m1 * id;
            auto bucket = std::size_t(h >> st().s1);
            auto index =
                std::size_t((std::uint64_t(st().m2[bucket]) * h) >> st().s2);
            st().control[index] = reinterpret_cast<type_id>(id);
        }
    }
}

template<std::size_t Lambda, std::size_t M1Candidates, std::size_t MaxTableBits>
template<class Registry>
void two_level_index<Lambda, M1Candidates, MaxTableBits>::fn<Registry>::check(
    std::size_t index, type_id type) {
    if (index >= st().slots || st().control[index] != type) {
        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

template<std::size_t Lambda, std::size_t M1Candidates, std::size_t MaxTableBits>
template<class Registry, class Stream>
auto two_level_index<Lambda, M1Candidates, MaxTableBits>::search_error::write(
    Stream& os) const -> void {
    os << "could not place " << classes << " type ids in a table of up to 2^"
       << table_bits << " slots\n";
}

} // namespace boost::openmethod::research

#endif
