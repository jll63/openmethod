// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.

#ifndef BOOST_OPENMETHOD_RESEARCH_VPTR_DIRECT_VECTOR_HPP
#define BOOST_OPENMETHOD_RESEARCH_VPTR_DIRECT_VECTOR_HPP

#include <boost/openmethod/preamble.hpp>

#include "direct_type_id_index.hpp"

#include <limits>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace boost::openmethod::research {

//! Store v-table pointers in a vector addressed by the type id itself.
//!
//! A @ref policies::vptr policy that fuses the subtraction of @ref
//! direct_type_id_index into the table's address. Splitting the two across the
//! `type_hash` seam cannot produce the code this is about: `vptr_vector` ends
//! in `vptrs[index]`, and a compiler cannot know that `type_id - base` is a
//! multiple of `sizeof(vptr_type)`, so it must emit the subtraction and the
//! shift before the scaled load.
//!
//! Here the state holds `origin = (const char*)vptrs.data() - min_type_id`,
//! and the lookup is `*(const vptr_type*)(origin + type_id)`: one load of
//! `origin` and one indexed load, with no arithmetic in between. The slot for
//! a type id sits at byte offset `type_id - min_type_id` from the start of the
//! vector, which works precisely because a slot is `sizeof(vptr_type)` bytes
//! wide and type ids are addresses of objects at least 16 bytes apart.
//!
//! Formally this is undefined behaviour: `origin` is not a pointer into the
//! vector, and the arithmetic relates unrelated objects. That is acceptable in
//! a prototype whose purpose is to measure the best case; a shippable version
//! would need a different formulation.
struct vptr_direct_vector : policies::vptr {

    static_assert(
        sizeof(vptr_type) == 8,
        "the fused form requires a v-table pointer the size of the stride");

    //! Same failure as @ref direct_type_id_index: the registered type ids are
    //! too spread out for a flat table.
    using too_sparse = direct_type_id_index<>::too_sparse;

    using errors = std::variant<too_sparse>;

    //! Refuse to allocate more slots than this per registered class.
    static constexpr std::size_t max_slots_per_class = 64;

    //! A VptrFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy
    template<class Registry>
    struct fn {
        //! What the vector holds: v-table pointers, or pointers to them if the
        //! registry contains the @ref policies::indirect_vptr policy.
        using element_type = std::conditional_t<
            Registry::has_indirect_vptr, const vptr_type*, vptr_type>;

        //! The policy's state.
        struct state {
            //! The v-table pointers, one per `sizeof(vptr_type)` bytes of the
            //! registered type ids' span.
            std::vector<element_type> vptrs;
            //! `vptrs.data()` biased by the lowest registered type id, so that
            //! a type id indexes the vector directly.
            const char* origin;
            //! The lowest and highest registered type ids, for the bounds
            //! check; unused when the registry has no runtime checks.
            detail::uintptr lo;
            detail::uintptr hi;
            //! The registered type ids, indexed like `vptrs`; empty unless the
            //! registry has runtime checks.
            std::vector<type_id> control;
        };

        //! Stores the v-table pointers.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @param options A tuple of option objects.
        template<class Context, class... Options>
        static auto initialize(
            const Context& ctx, const std::tuple<Options...>& options) -> void;

        //! Returns a *reference* to a v-table pointer for an object.
        //!
        //! @tparam Class A registered class.
        //! @param arg A reference to a const object of type `Class`.
        //! @return A reference to the v-table pointer for `Class`.
        template<class Class>
        BOOST_FORCEINLINE static auto dynamic_vptr(const Class& arg)
            -> const vptr_type& {
            return vptr(Registry::rtti::dynamic_type(arg));
        }

        //! Returns a *reference* to a v-table pointer for a type.
        //!
        //! @param type A `type_id`.
        //! @return A reference to the v-table pointer for `type`.
        BOOST_FORCEINLINE static auto vptr(type_id type) -> const vptr_type& {
            if constexpr (Registry::has_runtime_checks) {
                check(type);
            }

            auto slot = reinterpret_cast<const element_type*>(
                st().origin + reinterpret_cast<detail::uintptr>(type));

            if constexpr (Registry::has_indirect_vptr) {
                return **slot;
            } else {
                return *slot;
            }
        }

        //! Releases the memory allocated by `initialize`.
        //!
        //! @param options Zero or more option objects.
        template<class... Options>
        static auto finalize(const std::tuple<Options...>& options) -> void {
            (void)options;
            st().vptrs.clear();
            st().vptrs.shrink_to_fit();
            st().control.clear();
            st().control.shrink_to_fit();
            st().origin = nullptr;
        }

      private:
        static auto& st() {
            return Registry::template state<vptr_direct_vector>();
        }

        static void check(type_id type);
    };
};

template<class Registry>
template<class Context, class... Options>
auto vptr_direct_vector::fn<Registry>::initialize(
    const Context& ctx, const std::tuple<Options...>& options) -> void {
    (void)options;

    constexpr auto shift = direct_type_id_index<sizeof(vptr_type)>::shift;

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
        st().vptrs.clear();
        st().origin = nullptr;
        st().lo = st().hi = 0;

        return;
    }

    auto slots = std::size_t((hi - lo) >> shift) + 1;

    if (slots > max_slots_per_class * count) {
        too_sparse error;
        error.classes = count;
        error.slots = slots;
        error.span = std::size_t(hi - lo);

        if constexpr (Registry::has_error_handler) {
            Registry::error_handler::error(error);
        }

        abort();
    }

    st().vptrs.assign(slots, element_type{});
    st().lo = lo;
    st().hi = hi;

    if constexpr (Registry::has_runtime_checks) {
        st().control.assign(slots, type_id(detail::uintptr_max));
    }

    for (auto iter = ctx.classes_begin(); iter != ctx.classes_end(); ++iter) {
        for (auto type_iter = iter->type_id_begin();
             type_iter != iter->type_id_end(); ++type_iter) {
            auto index = std::size_t(
                (reinterpret_cast<detail::uintptr>(*type_iter) - lo) >> shift);

            if constexpr (Registry::has_indirect_vptr) {
                st().vptrs[index] = iter->static_vptr();
            } else {
                st().vptrs[index] = iter->vptr();
            }

            if constexpr (Registry::has_runtime_checks) {
                st().control[index] = *type_iter;
            }
        }
    }

    // Must be computed after the final resize: the vector's buffer moves.
    st().origin = reinterpret_cast<const char*>(st().vptrs.data()) - lo;
}

template<class Registry>
void vptr_direct_vector::fn<Registry>::check(type_id type) {
    constexpr auto shift = direct_type_id_index<sizeof(vptr_type)>::shift;
    auto value = reinterpret_cast<detail::uintptr>(type);

    if (value < st().lo || value > st().hi ||
        st().control[std::size_t((value - st().lo) >> shift)] != type) {

        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

} // namespace boost::openmethod::research

#endif
