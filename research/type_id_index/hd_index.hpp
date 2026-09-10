// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.

#ifndef BOOST_OPENMETHOD_RESEARCH_HD_INDEX_HPP
#define BOOST_OPENMETHOD_RESEARCH_HD_INDEX_HPP

#include <boost/openmethod/policies/fast_perfect_hash.hpp> // detail::uintptr
#include <boost/openmethod/preamble.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace boost::openmethod::research {

//! Map type ids to a dense index with a minimal perfect hash.
//!
//! A @ref type_hash policy in the "hash, displace and compress" form of
//! Belazzougui, Botelho and Dietzfelbinger, minus the compression:
//!
//! @code
//! h = x * seed;                          // one multiply
//! p = pilots[h >> bucket_shift];         // this bucket's pilot
//! index = mulhi64(h * p, slots);         // displace, then reduce
//! @endcode
//!
//! The type ids are split into `n / Lambda` buckets by the top bits of `h`.
//! Buckets are placed largest first; each is given a 32-bit odd *pilot*,
//! found by trial, such that multiplying the key by it sends every id in the
//! bucket to a slot that is still free. The final reduction is Lemire's
//! multiply-shift, the top half of a 64x64 product, which maps onto
//! `[0, slots)` for any `slots` without a division.
//!
//! The displacement has to be *multiplicative*. Adding a pilot, or xoring
//! one in, leaves the high bits of two keys in the same relation to each
//! other, and the reduction reads exactly those high bits: two ids that
//! agree there then collide under every pilot, and the bucket is
//! unplaceable for any of them. There are about `Lambda / 2` such pairs in a
//! table of `n` slots, so the search fails outright rather than slowly. A
//! multiplication has no such fixed points: `(h1 - h2) * p` sweeps the whole
//! word as `p` varies.
//!
//! The result is **near-minimal**: `m = n * 100 / LoadPercent` slots for `n`
//! type ids, so `vptr_vector` allocates `8m` bytes. That is the point.
//! @ref policies::fast_perfect_hash needs 1.25n-2.5n buckets, and both the
//! flat index and the `pext` cover pay for the *layout* of the addresses;
//! this one is `8m + 4n/Lambda` bytes whatever the addresses are.
//!
//! `LoadPercent = 100` - an exactly minimal table - is reachable but not in
//! bounded time: the last buckets have to hit the last few free slots, and
//! the expected number of trials for a bucket of size `s` facing a fraction
//! `phi` of free slots grows as `phi^-s`. A few percent of slack removes
//! that tail; buying exact minimality back needs CHD's compression step or
//! PTHash's free-slot remap, and both put work on the dispatch path that
//! costs far more than the slots they save.
//!
//! The price is on the dispatch path: the pilot has to be loaded before the
//! index can be formed, so the v-table lookup becomes two dependent loads
//! instead of one.
//!
//! The "compress" of CHD is deliberately dropped. Storing the pilots as a
//! compressed integer sequence is what makes CHD reach ~2 bits per key, but
//! decoding one on the dispatch path would cost far more than the table
//! saves. A pilot is stored raw, as the 32-bit value the hash xors in, so
//! reading it is a single indexed load.
//!
//! That is why this is `hd_index` and not `chd_index`: the published
//! algorithm's C is its compression step, and without it the name would
//! promise a footprint this does not have - ~2 bits per key against the
//! 9.45 bytes measured. The harness variants are `hd` and `hdmin`.
//!
//! Unlike @ref pext_index this needs no instruction-set extension: a
//! 64-bit multiply and a shift exist everywhere. It makes no assumption
//! about the *layout* of the type ids either, and none about the Itanium
//! ABI, so it is the only prototype here that does not depend on
//! `policies::std_rtti`. It does need every type id to be non-zero, which
//! addresses are: zero is a fixed point of the multiply, so a type id of 0
//! is pinned to slot 0 for every seed and every pilot, and the search fails
//! if some other bucket has taken that slot first. Hashing `x + 1` instead
//! of `x` would remove the restriction, at one instruction on the dispatch
//! path.
//!
//! @tparam Lambda Average bucket size. Larger means a smaller pilot table
//!   and a longer search.
//! @tparam LoadPercent Slots per 100 type ids, inverted: 100 is an exactly
//!   minimal table, 95 leaves one slot free in twenty.
//! @tparam MaxSeeds How many multipliers to try before giving up.
template<
    std::size_t Lambda = 4, std::size_t LoadPercent = 95,
    std::size_t MaxSeeds = 16>
struct hd_index : policies::type_hash {

    //! No seed yielded a complete assignment.
    struct search_error : openmethod_error {
        //! Number of registered type ids.
        std::size_t classes;
        //! Number of multipliers tried.
        std::size_t seeds;

        template<class Registry, class Stream>
        auto write(Stream& os) const -> void;
    };

    using errors = std::variant<search_error>;

    static_assert(Lambda > 0);
    static_assert(LoadPercent > 0 && LoadPercent <= 100);

    //! `state` layout when runtime checks are disabled.
    struct no_checks {
        //! The multiplier.
        std::uint64_t seed;
        //! `64 - log2(buckets)`.
        std::size_t bucket_shift;
        //! The number of slots.
        std::uint64_t size;
        //! One 32-bit pilot per bucket.
        std::vector<std::uint32_t> pilots;
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
            return Registry::template state<hd_index>();
        }

        static void check(std::size_t index, type_id type);

        // The pilot tried at step `k`. Multiplying by an odd constant is a
        // bijection on 32 bits, so the sequence walks the whole range; the
        // low bit is set because an even pilot loses the key's high bits.
        static auto pilot_at(std::uint32_t k) -> std::uint32_t {
            return (k * 0x9e3779b9u) | 1u;
        }

        // The top half of a 64x64 product.
        static auto mulhi(std::uint64_t a, std::uint64_t b) -> std::uint64_t {
#if defined(__SIZEOF_INT128__)
            return std::uint64_t((static_cast<__uint128_t>(a) * b) >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
            return __umulh(a, b);
#else
            auto lo = [](std::uint64_t v) { return v & 0xffffffffull; };
            auto hi = [](std::uint64_t v) { return v >> 32; };
            auto ll = lo(a) * lo(b);
            auto lh = lo(a) * hi(b);
            auto hl = hi(a) * lo(b);
            auto hh = hi(a) * hi(b);
            auto mid = hi(ll) + lo(lh) + lo(hl);

            return hh + hi(lh) + hi(hl) + hi(mid);
#endif
        }

        // Where a pilot sends a key.
        static auto place(
            std::uint64_t h, std::uint32_t pilot, std::uint64_t slots)
            -> std::size_t {
            return std::size_t(mulhi(h * pilot, slots));
        }

        static auto mix_seed(std::uint64_t z) -> std::uint64_t {
            z += 0x9e3779b97f4a7c15ull;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;

            return (z ^ (z >> 31)) | 1;
        }

        static auto build(
            const std::vector<std::uint64_t>& ids, std::uint64_t seed,
            std::size_t bucket_shift, std::uint64_t slots,
            std::vector<std::uint32_t>& pilots) -> bool;

      public:
        //! Finds a seed and the pilots.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @param options A tuple of option objects.
        template<class Context, class... Options>
        static auto initialize(
            const Context& ctx, const std::tuple<Options...>& options) -> void;

        //! Bytes of pilot table, over and above the `8 * slots` that
        //! `vptr_vector` allocates.
        static auto aux_bytes() -> std::size_t {
            return st().pilots.size() * sizeof(std::uint32_t);
        }

        //! Returns the hash range: `[0, slots - 1]`.
        static auto hash_range() -> std::pair<std::size_t, std::size_t> {
            return std::pair{
                std::size_t(0), std::size_t(st().size ? st().size - 1 : 0)};
        }

        //! Map a type id to an index
        //!
        //! @param type The type_id to map
        //! @return The index
        BOOST_FORCEINLINE
        static auto hash(type_id type) -> std::size_t {
            auto h = std::uint64_t(reinterpret_cast<detail::uintptr>(type)) *
                st().seed;
            auto pilot = st().pilots[std::size_t(h >> st().bucket_shift)];
            auto index = place(h, pilot, st().size);

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

template<std::size_t Lambda, std::size_t LoadPercent, std::size_t MaxSeeds>
template<class Registry>
auto hd_index<Lambda, LoadPercent, MaxSeeds>::fn<Registry>::build(
    const std::vector<std::uint64_t>& ids, std::uint64_t seed,
    std::size_t bucket_shift, std::uint64_t slots,
    std::vector<std::uint32_t>& pilots) -> bool {
    auto n = ids.size();
    auto buckets = pilots.size();

    // Bucket each id, and keep the hashed key the pilot will displace.
    std::vector<std::uint64_t> keys(n);
    std::vector<std::uint32_t> bucket_of(n);

    for (std::size_t i = 0; i != n; ++i) {
        auto h = ids[i] * seed;
        bucket_of[i] = std::uint32_t(h >> bucket_shift);
        keys[i] = h;
    }

    // Group by bucket: counting sort into a CSR-style pair of arrays.
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

    // The last buckets face a nearly full table. A bucket of size `s` with a
    // fraction `phi` of the slots free needs about `phi^-s` trials, so the
    // budget has to be generous; it is a diagnostic, not a working limit.
    const std::uint32_t max_tries = 1u << 20;

    std::vector<char> occupied(std::size_t(slots), 0);
    std::vector<std::size_t> placed;
    placed.reserve(64);

    for (auto b : order) {
        auto first = start[b], last = start[b + 1];

        if (first == last) {
            pilots[b] = 0;

            continue;
        }

        bool done = false;

        for (std::uint32_t k = 0; k != max_tries; ++k) {
            auto pilot = pilot_at(k);
            placed.clear();
            bool ok = true;

            for (auto m = first; m != last; ++m) {
                auto index = place(keys[members[m]], pilot, slots);

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

                pilots[b] = pilot;
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

template<std::size_t Lambda, std::size_t LoadPercent, std::size_t MaxSeeds>
template<class Registry>
template<class Context, class... Options>
auto hd_index<Lambda, LoadPercent, MaxSeeds>::fn<Registry>::initialize(
    const Context& ctx, const std::tuple<Options...>& options) -> void {
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
        st().seed = 1;
        st().bucket_shift = 63;
        st().size = 0;
        st().pilots.assign(2, 0);

        return;
    }

    // Slots. `LoadPercent = 100` asks for exactly one per type id.
    auto slots = (n * 100 + LoadPercent - 1) / LoadPercent;

    // Smallest power of two of at least ceil(n / Lambda) buckets, and never
    // fewer than two, so the shift stays below 64.
    std::size_t buckets = 2;
    std::size_t log_buckets = 1;

    while (buckets * Lambda < n) {
        buckets <<= 1;
        ++log_buckets;
    }

    std::vector<std::uint32_t> pilots(buckets);
    std::size_t bucket_shift = 64 - log_buckets;
    std::size_t seeds = 0;
    bool found = false;

    for (; seeds != MaxSeeds; ++seeds) {
        auto seed = mix_seed(seeds);

        if (build(ids, seed, bucket_shift, slots, pilots)) {
            st().seed = seed;
            found = true;
            ++seeds;

            break;
        }
    }

    if (!found) {
        search_error error;
        error.classes = n;
        error.seeds = seeds;

        if constexpr (Registry::has_error_handler) {
            Registry::error_handler::error(error);
        }

        abort();
    }

    st().bucket_shift = bucket_shift;
    st().size = slots;
    st().pilots = std::move(pilots);

    if constexpr (Context::template has_option<trace>) {
        ctx.tr << "  type ids: " << n << ", buckets: " << buckets
               << ", seeds tried: " << seeds << ", table: " << slots
               << " slots + " << buckets << " pilots\n";
    }

    if constexpr (Registry::has_runtime_checks) {
        st().control.assign(std::size_t(slots), type_id(detail::uintptr_max));

        for (auto id : ids) {
            auto h = id * st().seed;
            auto pilot = st().pilots[std::size_t(h >> bucket_shift)];
            st().control[place(h, pilot, slots)] =
                reinterpret_cast<type_id>(id);
        }
    }
}

template<std::size_t Lambda, std::size_t LoadPercent, std::size_t MaxSeeds>
template<class Registry>
void hd_index<Lambda, LoadPercent, MaxSeeds>::fn<Registry>::check(
    std::size_t index, type_id type) {
    if (index >= st().size || st().control[index] != type) {
        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

template<std::size_t Lambda, std::size_t LoadPercent, std::size_t MaxSeeds>
template<class Registry, class Stream>
auto hd_index<Lambda, LoadPercent, MaxSeeds>::search_error::write(
    Stream& os) const -> void {
    os << "could not place " << classes << " type ids after trying " << seeds
       << " multipliers\n";
}

} // namespace boost::openmethod::research

#endif
