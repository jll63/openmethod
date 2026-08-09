// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_BOOST_ANY_HPP
#define BOOST_OPENMETHOD_INTEROP_BOOST_ANY_HPP

#include <boost/any.hpp>
#include <boost/openmethod/core.hpp>
#include <boost/openmethod/interop/virtual_any.hpp>

namespace boost::openmethod {

namespace detail {

template<class Registry>
struct validate_method_parameter<virtual_<const boost::any&>, Registry, void>
    : std::true_type {};

template<class Registry>
struct validate_method_parameter<virtual_<boost::any&>, Registry, void>
    : std::true_type {};

template<class Registry>
struct validate_method_parameter<virtual_<boost::any&&>, Registry, void>
    : std::true_type {};

} // namespace detail

//! Specialize virtual_traits for `const boost::any&` (const reference).
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `boost::any::type()`.
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<const boost::any&, Registry> {
    //! The type used for dispatch.
    using virtual_type = boost::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `boost::any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::any& arg) -> const boost::any& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for an object.
    //!
    //! Acquires the dynamic @ref type_id of the value stored in `arg`, using
    //! `boost::any::type()`. This requires the registry's @ref rtti policy to
    //! identify classes by `&typeid(T)`, as @ref std_rtti does;
    //! `boost::any::type()` yields the same `std::type_info` object, provided
    //! Boost.TypeIndex uses `stl_type_index`.
    //!
    //! Passes the type id to the registry's @ref policies::vptr policy, which
    //! must provide @ref policies::VptrFn::vptr. Both
    //! @ref policies::vptr_vector and @ref policies::vptr_map do.
    //!
    //! If the registry has a @ref type_hash policy, uses it to convert the
    //! type id to an index; otherwise, uses the type_id as the index.
    //!
    //! If the registry contains the @ref runtime_checks policy, verifies
    //! that the index falls within the limits of the vector. If it does
    //! not, and if the registry contains a @ref error_handler policy, calls
    //! its @ref error function with a @ref missing_class value, then
    //! terminates the program with @ref abort.
    //!
    //! @param arg A reference to a const `any`.
    //! @return A reference to the v-table pointer for the stored value.
    static auto vptr(const boost::any& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&arg.type());
    }

    //! Cast to a type.
    //!
    //! If `U` is the `any` itself (by any reference category), returns
    //! `arg` unchanged, which is how a catch-all overrider is written.
    //! Otherwise, extracts the stored value using `boost::any_cast`.
    //!
    //! Since the `any` argument is const, `U` cannot be a mutable reference.
    //! `boost::any_cast` rewrites `U` to a const reference for a const `any`,
    //! and would fail inside Boost.Any; this overload is removed from the
    //! overload set instead.
    //!
    //! @tparam U The target type (e.g. `const Dog&`, `Dog`).
    //! @param arg A reference to a const `boost::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<
        typename U,
        typename = std::enable_if_t<
            !std::is_reference_v<U> ||
            std::is_const_v<std::remove_reference_t<U>>>>
    static auto cast(const boost::any& arg) -> decltype(auto) {
        if constexpr (std::is_same_v<
                          std::remove_cv_t<std::remove_reference_t<U>>,
                          boost::any>) {
            return (arg);
        } else {
            return boost::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for `boost::any&` (mutable reference).
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `boost::any::type()`.
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<boost::any&, Registry> {
    //! The type used for dispatch.
    using virtual_type = boost::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `boost::any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::any& arg) -> const boost::any& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for an object.
    //!
    //! Acquires the dynamic @ref type_id of the value stored in `arg`, using
    //! `boost::any::type()`. This requires the registry's @ref rtti policy to
    //! identify classes by `&typeid(T)`, as @ref std_rtti does;
    //! `boost::any::type()` yields the same `std::type_info` object, provided
    //! Boost.TypeIndex uses `stl_type_index`.
    //!
    //! Passes the type id to the registry's @ref policies::vptr policy, which
    //! must provide @ref policies::VptrFn::vptr. Both
    //! @ref policies::vptr_vector and @ref policies::vptr_map do.
    //!
    //! If the registry has a @ref type_hash policy, uses it to convert the
    //! type id to an index; otherwise, uses the type_id as the index.
    //!
    //! If the registry contains the @ref runtime_checks policy, verifies
    //! that the index falls within the limits of the vector. If it does
    //! not, and if the registry contains a @ref error_handler policy, calls
    //! its @ref error function with a @ref missing_class value, then
    //! terminates the program with @ref abort.
    //!
    //! @param arg A reference to a `boost::any`.
    //! @return A reference to the v-table pointer for the stored value.
    static auto vptr(const boost::any& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&arg.type());
    }

    //! Cast to a type.
    //!
    //! If `U` is the `any` itself (by any reference category), returns
    //! `arg` unchanged, which is how a catch-all overrider is written.
    //! Otherwise, extracts the stored value using `boost::any_cast`. Supports mutable
    //! references (e.g. `Dog&`) because the `any` argument is not const;
    //! modifications through the result are visible through the `any`.
    //!
    //! `U` cannot be an rvalue reference. Unlike `std::any_cast`,
    //! `boost::any_cast` binds an rvalue reference to the value stored in an
    //! lvalue `any`; moving the value out must go through an explicit
    //! `virtual_<boost::any&&>` parameter, so this overload is removed from
    //! the overload set.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg A mutable reference to the `boost::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<
        typename U, typename = std::enable_if_t<!std::is_rvalue_reference_v<U>>>
    static auto cast(boost::any& arg) -> decltype(auto) {
        if constexpr (std::is_same_v<
                          std::remove_cv_t<std::remove_reference_t<U>>,
                          boost::any>) {
            return (arg);
        } else {
            return boost::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for `boost::any&&` (xvalue reference).
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `boost::any::type()`.
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<boost::any&&, Registry> {
    //! The type used for dispatch.
    using virtual_type = boost::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `boost::any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::any& arg) -> const boost::any& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for an object.
    //!
    //! Acquires the dynamic @ref type_id of the value stored in `arg`, using
    //! `boost::any::type()`. This requires the registry's @ref rtti policy to
    //! identify classes by `&typeid(T)`, as @ref std_rtti does;
    //! `boost::any::type()` yields the same `std::type_info` object, provided
    //! Boost.TypeIndex uses `stl_type_index`.
    //!
    //! Passes the type id to the registry's @ref policies::vptr policy, which
    //! must provide @ref policies::VptrFn::vptr. Both
    //! @ref policies::vptr_vector and @ref policies::vptr_map do.
    //!
    //! If the registry has a @ref type_hash policy, uses it to convert the
    //! type id to an index; otherwise, uses the type_id as the index.
    //!
    //! If the registry contains the @ref runtime_checks policy, verifies
    //! that the index falls within the limits of the vector. If it does
    //! not, and if the registry contains a @ref error_handler policy, calls
    //! its @ref error function with a @ref missing_class value, then
    //! terminates the program with @ref abort.
    //!
    //! @param arg A reference to a `boost::any`.
    //! @return A reference to the v-table pointer for the stored value.
    static auto vptr(const boost::any& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&arg.type());
    }

    //! Cast to a type.
    //!
    //! If `U` is the `any` itself (by any reference category), returns
    //! `arg` unchanged, which is how a catch-all overrider is written.
    //! Otherwise, extracts the stored value using `boost::any_cast`.
    //!
    //! `U` cannot be a mutable lvalue reference: that would bind a reference
    //! to the value contained in a temporary. Boost.Any rejects it with a
    //! static assertion; this overload is removed from the overload set
    //! instead, for consistency with the other reference categories.
    //!
    //! @tparam U The target type (e.g. `Dog&&`, `const Dog&`, `Dog`).
    //! @param arg An rvalue reference to the `boost::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<
        typename U,
        typename = std::enable_if_t<
            !std::is_lvalue_reference_v<U> ||
            std::is_const_v<std::remove_reference_t<U>>>>
    static auto cast(boost::any&& arg) -> decltype(auto) {
        if constexpr (std::is_same_v<
                          std::remove_cv_t<std::remove_reference_t<U>>,
                          boost::any>) {
            return std::move(arg);
        } else {
            return boost::any_cast<U>(std::move(arg));
        }
    }
};

//! Register the types that a `boost::any` virtual parameter may contain.
//!
//! Registers `boost::any` as a class, and each `T` as a class derived from
//! `boost::any`. This makes the contained types visible to the dispatch
//! machinery, which resolves a call on the `type_id` returned by
//! `boost::any::type()`.
//!
//! The root class is `boost::any`, distinct from the one used by
//! @ref use_std_any_types for `std::any`, so both may be used in the same
//! program, and with the same registry.
//!
//! @tparam T... The types that may be stored in the `any`, optionally
//! followed by a @ref registry.
//!
//! @par Example
//! include:virtual_any.cpp#boost_classes;boost_dispatch
//!
//! @see [Interoperation with `any`](xref:ROOT:interop_any.adoc)
template<typename... T>
struct use_boost_any_types
    : detail::use_any_types_aux<
          typename detail::extract_registry<T...>::registry, boost::any,
          typename detail::extract_registry<T...>::others> {};

//! Alias for a `virtual_any<boost::any>`, in the default registry.
//!
//! With another registry, use `virtual_any<boost::any, Registry>` directly.
//!
//! @see [Interoperation with `any`](xref:ROOT:interop_any.adoc)
using virtual_boost_any = virtual_any<boost::any>;

// The primary final_virtual_ptr would silently use static_vptr<boost::any>
// - the v-table of the `any` root class, not of the contained value.
// Delete the combination. Both call forms need covering: the non-template
// overloads catch calls that deduce the default registry, and are removed
// from consideration when an explicit template argument list is given, so
// the Registry-only templates - more specialized than the primary - catch
// those.
//
// Hidden from the reference: they are a guard, not API, and six deleted
// overloads would crowd the `final_virtual_ptr` overload list.

#ifndef __MRDOCS__
template<class Registry>
void final_virtual_ptr(const boost::any&) = delete;
template<class Registry>
void final_virtual_ptr(boost::any&) = delete;
template<class Registry>
void final_virtual_ptr(boost::any&&) = delete;
void final_virtual_ptr(const boost::any&) = delete;
void final_virtual_ptr(boost::any&) = delete;
void final_virtual_ptr(boost::any&&) = delete;
#endif

namespace aliases {
using boost::openmethod::use_boost_any_types;
using boost::openmethod::virtual_boost_any;
} // namespace aliases

} // namespace boost::openmethod

#endif
