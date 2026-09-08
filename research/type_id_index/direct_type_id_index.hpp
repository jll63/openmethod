// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.

#ifndef BOOST_OPENMETHOD_RESEARCH_DIRECT_TYPE_ID_INDEX_HPP
#define BOOST_OPENMETHOD_RESEARCH_DIRECT_TYPE_ID_INDEX_HPP

#include <boost/openmethod/policies/fast_perfect_hash.hpp>
#include <boost/openmethod/preamble.hpp>

#include <limits>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace boost::openmethod::research {

//! Map type ids to indexes by subtraction and division.
//!
//! A @ref type_hash policy in the form `H(x) = (x - base) / Divisor`, where
//! `base` is the lowest registered type id. It replaces the multiplicative
//! perfect hash of @ref policies::fast_perfect_hash, which must *search* for
//! its factors, with a min/max scan.
//!
//! The map is injective for every `Divisor` up to 16, unconditionally, when
//! the type ids are the addresses of `std::type_info` objects: the smallest
//! Itanium ABI record (`__class_type_info`) is 16 bytes, distinct objects do
//! not overlap, so two distinct type ids differ by at least 16; and `a - b >=
//! d` implies `a / d > b / d`. There is therefore no search, and no failure
//! mode except memory.
//!
//! Memory is the whole question. The index range is the *span* of the
//! registered type ids divided by `Divisor`, and nothing constrains the span:
//! type_info records are interleaved with vtables inside one module, and
//! different modules are mapped far apart. `MaxSlotsPerClass` bounds the
//! resulting waste; beyond it the policy reports @ref too_sparse rather than
//! allocate.
//!
//! This policy assumes the type ids are object addresses, i.e. @ref
//! policies::std_rtti. A custom RTTI handing out small integers, or addresses
//! of objects smaller than `Divisor`, breaks the injectivity argument.
//!
//! @tparam Divisor The divisor, a power of two, at most 16.
//! @tparam MaxSlotsPerClass Refuse to allocate more slots per registered
//! class than this.
template<
    std::size_t Divisor = sizeof(vptr_type), std::size_t MaxSlotsPerClass = 64>
struct direct_type_id_index : policies::type_hash {

    static_assert(
        Divisor != 0 && (Divisor & (Divisor - 1)) == 0,
        "Divisor must be a power of two");
    static_assert(
        Divisor <= 16,
        "the injectivity argument requires a divisor no larger than the "
        "smallest type_info record");

    static constexpr std::size_t shift = [] {
        std::size_t result = 0;

        for (auto value = Divisor; value > 1; value >>= 1) {
            ++result;
        }

        return result;
    }();

    //! The registered type ids are too spread out.
    //!
    //! The span of the registered type ids, divided by `Divisor`, would
    //! require more than `MaxSlotsPerClass` slots per registered class. In
    //! practice this means the registered classes come from modules that the
    //! loader mapped far apart.
    struct too_sparse : openmethod_error {
        //! Number of registered type ids.
        std::size_t classes;
        //! Number of slots the index range would need.
        std::size_t slots;
        //! Distance between the lowest and the highest type id, in bytes.
        std::size_t span;

        //! Write a short description to an output stream
        //! @param os The output stream
        //! @tparam Registry The registry
        //! @tparam Stream A @ref LightweightOutputStream
        template<class Registry, class Stream>
        auto write(Stream& os) const -> void;
    };

    using errors = std::variant<too_sparse>;

    //! `state` layout when runtime checks are disabled.
    struct no_checks {
        //! The lowest registered type id.
        detail::uintptr base;
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
            return Registry::template state<direct_type_id_index>();
        }

        static void check(std::size_t index, type_id type);

      public:
        //! Finds the base.
        //!
        //! Scans the registered type ids for their minimum and maximum, and
        //! derives the index range from them. Unlike @ref
        //! policies::fast_perfect_hash::initialize, this cannot fail to find a
        //! mapping; it fails only if the mapping would need more than
        //! `MaxSlotsPerClass` slots per class, in which case it reports @ref
        //! too_sparse then calls `abort`.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @param options A tuple of option objects.
        template<class Context, class... Options>
        static auto initialize(
            const Context& ctx, const std::tuple<Options...>& options) -> void;

        //! Returns the hash range
        //!
        //! Always starts at zero: `hash` returns a *rebased* index. That is
        //! not a refinement, it is what makes the policy usable at all -
        //! `vptr_vector` sizes its vector from `hash_range().second + 1`, and
        //! an un-rebased `type_id / Divisor` is of the order of 2^44.
        static auto hash_range() -> std::pair<std::size_t, std::size_t> {
            return std::pair{std::size_t(0), st().max_value};
        }

        //! Map a type id to an index
        //!
        //! If `Registry` contains the @ref policies::runtime_checks policy,
        //! checks that the type id was present in the set passed to @ref
        //! initialize, and reports a @ref missing_class if it was not.
        //!
        //! @param type The type_id to map
        //! @return The index
        BOOST_FORCEINLINE
        static auto hash(type_id type) -> std::size_t {
            auto index =
                (reinterpret_cast<detail::uintptr>(type) - st().base) >> shift;

            if constexpr (Registry::has_runtime_checks) {
                check(index, type);
            }

            return index;
        }

        //! Releases the control table.
        //!
        //! @param options A tuple of option objects.
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

template<std::size_t Divisor, std::size_t MaxSlotsPerClass>
template<class Registry>
template<class Context, class... Options>
auto direct_type_id_index<Divisor, MaxSlotsPerClass>::fn<Registry>::initialize(
    const Context& ctx, const std::tuple<Options...>& options) -> void {
    (void)options;

    auto lo = (std::numeric_limits<detail::uintptr>::max)();
    auto hi = (std::numeric_limits<detail::uintptr>::min)();
    std::size_t count = 0;

    for (auto iter = ctx.classes_begin(); iter != ctx.classes_end(); ++iter) {
        for (auto type_iter = iter->type_id_begin();
             type_iter != iter->type_id_end(); ++type_iter) {
            auto type = reinterpret_cast<detail::uintptr>(*type_iter);
            lo = (std::min)(lo, type);
            hi = (std::max)(hi, type);
            ++count;
        }
    }

    if (count == 0) {
        st().base = 0;
        st().max_value = 0;

        return;
    }

    auto span = hi - lo;
    auto slots = std::size_t(span >> shift) + 1;

    // This is also the overflow guard for slots * sizeof(vptr_type).
    if (slots > MaxSlotsPerClass * count) {
        too_sparse error;
        error.classes = count;
        error.slots = slots;
        error.span = std::size_t(span);

        if constexpr (Registry::has_error_handler) {
            Registry::error_handler::error(error);
        }

        abort();
    }

    st().base = lo;
    st().max_value = slots - 1;

    if constexpr (Context::template has_option<trace>) {
        ctx.tr << "  type ids: " << count << ", span = " << std::size_t(span)
               << ", slots = " << slots << "\n";
    }

    // A class may have several class_info entries carrying the same type_id,
    // one per module (see augment_classes() in initialize.hpp). Unlike
    // fast_perfect_hash, which must tell that apart from a real collision,
    // this policy needs no special case: the map is injective by construction,
    // so a repeated type id necessarily lands on the slot it already occupies.
    // The other multi-module case - one class with *different* type ids in
    // different modules, under hidden visibility - also works: the two slots
    // simply receive the same v-table pointer.
    if constexpr (Registry::has_runtime_checks) {
        st().control.assign(slots, type_id(detail::uintptr_max));

        for (auto iter = ctx.classes_begin(); iter != ctx.classes_end();
             ++iter) {
            for (auto type_iter = iter->type_id_begin();
                 type_iter != iter->type_id_end(); ++type_iter) {
                // Not hash(): under runtime checks it validates against this
                // very table, which is not filled in yet.
                st().control[std::size_t(
                    (reinterpret_cast<detail::uintptr>(*type_iter) - lo) >>
                    shift)] = *type_iter;
            }
        }
    }
}

template<std::size_t Divisor, std::size_t MaxSlotsPerClass>
template<class Registry>
void direct_type_id_index<Divisor, MaxSlotsPerClass>::fn<Registry>::check(
    std::size_t index, type_id type) {
    if (index > st().max_value || st().control[index] != type) {
        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

template<std::size_t Divisor, std::size_t MaxSlotsPerClass>
template<class Registry, class Stream>
auto direct_type_id_index<Divisor, MaxSlotsPerClass>::too_sparse::write(
    Stream& os) const -> void {
    os << "type ids of " << classes << " classes span " << span
       << " bytes, which would need " << slots
       << " slots; they are probably spread across modules mapped far apart\n";
}

} // namespace boost::openmethod::research

#endif
