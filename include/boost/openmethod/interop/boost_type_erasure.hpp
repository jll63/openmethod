// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_BOOST_TYPE_ERASURE_HPP
#define BOOST_OPENMETHOD_INTEROP_BOOST_TYPE_ERASURE_HPP

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/any_cast.hpp>
#include <boost/type_erasure/concept_of.hpp>
#include <boost/type_erasure/typeid_of.hpp>

#include <boost/openmethod/core.hpp>
#include <boost/openmethod/interop/virtual_any.hpp>

#include <type_traits>
#include <utility>

// Dispatch on the type contained in a boost::type_erasure::any.
//
// The Concept must contain boost::type_erasure::typeid_<> - which
// `relaxed` already implies - so that `typeid_of` can identify the
// contained value. Dispatch is on the `std::type_info` object returned by
// `typeid_of`: the type of the contained value for the owning flavor
// (`any<Concept>`), or the type *bound at construction* for the reference
// flavors (`any<Concept, _self&>`, `any<Concept, const _self&>`) - never
// the C++ RTTI dynamic type of the referent.
//
// Supported virtual parameter forms:
// - `virtual_<const any<Concept>&>`, `virtual_<any<Concept>&>`,
//   `virtual_<any<Concept>&&>` - the owning flavor, by reference, like
//   `std::any`;
// - `virtual_<any<Concept, _self&>>` and
//   `virtual_<any<Concept, const _self&>>` - the reference-wrapper
//   flavors, by value (they are cheap, two-word handles);
// - `virtual_any<any<Concept>>` - looks the v-table pointer up once, at
//   construction. The Concept needs `relaxed` for virtual_any's default
//   constructor and assignment, and `copy_constructible<>` for copies.
//
// The rvalue-reference flavor (`any<Concept, _self&&>`), and placeholders
// other than `_self`, are not supported.

namespace boost::openmethod {

namespace detail {

// Classification of the placeholder of an any<Concept, T>. `T = _self`
// (or any non-reference placeholder): the any owns the value. `T =
// _self&`: non-owning handle to a mutable referent. Anything else
// (`const _self&`, `_self&&`) is treated as binding a value that may not
// be mutated or moved from.
template<typename T>
constexpr bool te_owning = !std::is_reference_v<T>;

template<typename T>
constexpr bool te_mutable_bound = std::is_lvalue_reference_v<T> &&
    !std::is_const_v<std::remove_reference_t<T>>;

// Does U, an overrider parameter type, require mutable access?
template<typename U>
constexpr bool te_mutable_target = std::is_lvalue_reference_v<U> &&
    !std::is_const_v<std::remove_reference_t<U>>;

// Is U, an overrider parameter type, the `any` itself (by value or by
// any reference category)? Then the argument is passed through
// unchanged - the catch-all overrider case - instead of going through
// any_cast, which would throw unless the any contains an any.
template<typename U, class Any>
constexpr bool te_pass_through =
    std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, Any>;

// The canonical root class for a Concept: the owning flavor. All the
// virtual_traits below use it as their virtual_type, whatever the
// flavor of the parameter, so methods, overriders and
// use_type_erasure_types agree on a single registered root per Concept.
template<class Any>
using type_erasure_root = boost::type_erasure::any<
    typename boost::type_erasure::concept_of<Any>::type>;

template<class C, typename T, class Registry>
struct validate_method_parameter<
    virtual_<const boost::type_erasure::any<C, T>&>, Registry, void>
    : std::true_type {};

template<class C, typename T, class Registry>
struct validate_method_parameter<
    virtual_<boost::type_erasure::any<C, T>&>, Registry, void>
    : std::true_type {};

template<class C, typename T, class Registry>
struct validate_method_parameter<
    virtual_<boost::type_erasure::any<C, T>&&>, Registry, void>
    : std::true_type {};

template<class C, typename T, class Registry>
struct validate_method_parameter<
    virtual_<boost::type_erasure::any<C, T&>>, Registry, void>
    : std::true_type {};

template<class C, typename T, class Registry>
struct validate_method_parameter<
    virtual_<boost::type_erasure::any<C, const T&>>, Registry, void>
    : std::true_type {};

template<class C, class Registry>
struct validate_method_parameter<
    virtual_<boost::type_erasure::any<C, boost::type_erasure::_self>>, Registry,
    void> : std::false_type {
    static_assert(
        false_t<C>, "an owning type_erasure::any must be passed by reference");
};

} // namespace detail

//! Specialize virtual_traits for `const boost::type_erasure::any&`.
//!
//! Dispatch is based on the type of the value bound to the `any`,
//! obtained via `boost::type_erasure::typeid_of`. `Concept` must contain
//! `boost::type_erasure::typeid_<>`; `relaxed` implies it.
//!
//! This specialization serves the owning flavor (`any<Concept>`) and,
//! through a const wrapper, the reference flavors.
//!
//! @tparam C The `any`'s Concept.
//! @tparam T The `any`'s placeholder.
//! @tparam Registry A @ref registry.
template<class C, typename T, class Registry>
struct virtual_traits<const boost::type_erasure::any<C, T>&, Registry> {
    //! The type used for dispatch: the owning flavor for `C`.
    using virtual_type = boost::type_erasure::any<C>;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to an `any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::type_erasure::any<C, T>& arg)
        -> const boost::type_erasure::any<C, T>& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for the bound value.
    //!
    //! Looks up the @ref type_id returned by
    //! `boost::type_erasure::typeid_of` in the registry's `vptr` policy.
    //!
    //! @param arg A reference to a const `any`.
    //! @return A reference to the v-table pointer for the bound value.
    static auto
    vptr(const boost::type_erasure::any<C, T>& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&boost::type_erasure::typeid_of(arg));
    }

    //! Cast to a type.
    //!
    //! Extracts the bound value using `boost::type_erasure::any_cast`.
    //! Since the `any` is const, `U` can be a mutable reference only for
    //! the mutable-reference flavor (`any<Concept, _self&>`), whose
    //! referent stays mutable through a const wrapper. Rvalue references
    //! are never allowed; the overloads are removed from the overload
    //! set.
    //!
    //! @tparam U The target type (e.g. `const Dog&`, `Dog`).
    //! @param arg A reference to a const `any` method argument.
    //! @return The value bound to `arg`, cast to `U`.
    template<
        typename U,
        typename = std::enable_if_t<
            !std::is_rvalue_reference_v<U> &&
            (!detail::te_mutable_target<U> || detail::te_mutable_bound<T>)>>
    static auto
    cast(const boost::type_erasure::any<C, T>& arg) -> decltype(auto) {
        if constexpr (detail::te_pass_through<
                          U, boost::type_erasure::any<C, T>>) {
            return (arg);
        } else {
            return boost::type_erasure::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for `boost::type_erasure::any&` (mutable
//! reference).
//!
//! Dispatch is based on the type of the value bound to the `any`,
//! obtained via `boost::type_erasure::typeid_of`. `Concept` must contain
//! `boost::type_erasure::typeid_<>`; `relaxed` implies it.
//!
//! @tparam C The `any`'s Concept.
//! @tparam T The `any`'s placeholder.
//! @tparam Registry A @ref registry.
template<class C, typename T, class Registry>
struct virtual_traits<boost::type_erasure::any<C, T>&, Registry> {
    //! The type used for dispatch: the owning flavor for `C`.
    using virtual_type = boost::type_erasure::any<C>;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to an `any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::type_erasure::any<C, T>& arg)
        -> const boost::type_erasure::any<C, T>& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for the bound value.
    //!
    //! Looks up the @ref type_id returned by
    //! `boost::type_erasure::typeid_of` in the registry's `vptr` policy.
    //!
    //! @param arg A reference to an `any`.
    //! @return A reference to the v-table pointer for the bound value.
    static auto
    vptr(const boost::type_erasure::any<C, T>& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&boost::type_erasure::typeid_of(arg));
    }

    //! Cast to a type.
    //!
    //! Extracts the bound value using `boost::type_erasure::any_cast`.
    //! Supports mutable references (e.g. `Dog&`), except through the
    //! const-reference flavor (`any<Concept, const _self&>`). `U` cannot
    //! be an rvalue reference: moving the value out must go through an
    //! explicit rvalue-reference parameter. The disallowed overloads are
    //! removed from the overload set.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg A mutable reference to the `any` method argument.
    //! @return The value bound to `arg`, cast to `U`.
    template<
        typename U,
        typename = std::enable_if_t<
            !std::is_rvalue_reference_v<U> &&
            (!detail::te_mutable_target<U> || detail::te_owning<T> ||
             detail::te_mutable_bound<T>)>>
    static auto cast(boost::type_erasure::any<C, T>& arg) -> decltype(auto) {
        if constexpr (detail::te_pass_through<
                          U, boost::type_erasure::any<C, T>>) {
            return (arg);
        } else {
            return boost::type_erasure::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for `boost::type_erasure::any&&` (xvalue
//! reference).
//!
//! Dispatch is based on the type of the value bound to the `any`,
//! obtained via `boost::type_erasure::typeid_of`. `Concept` must contain
//! `boost::type_erasure::typeid_<>`; `relaxed` implies it.
//!
//! @tparam C The `any`'s Concept.
//! @tparam T The `any`'s placeholder.
//! @tparam Registry A @ref registry.
template<class C, typename T, class Registry>
struct virtual_traits<boost::type_erasure::any<C, T>&&, Registry> {
    //! The type used for dispatch: the owning flavor for `C`.
    using virtual_type = boost::type_erasure::any<C>;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to an `any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::type_erasure::any<C, T>& arg)
        -> const boost::type_erasure::any<C, T>& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for the bound value.
    //!
    //! Looks up the @ref type_id returned by
    //! `boost::type_erasure::typeid_of` in the registry's `vptr` policy.
    //!
    //! @param arg A reference to a const `any`.
    //! @return A reference to the v-table pointer for the bound value.
    static auto
    vptr(const boost::type_erasure::any<C, T>& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&boost::type_erasure::typeid_of(arg));
    }

    //! Cast to a type.
    //!
    //! Extracts the bound value using `boost::type_erasure::any_cast`.
    //! `boost::type_erasure::any_cast` has no rvalue overload, so, for an
    //! rvalue-reference `U`, the result of a mutable-reference cast is
    //! moved - only for the owning flavor, since the rvalue-ness of a
    //! reference wrapper says nothing about the referent. Casting to a
    //! value also moves for the owning flavor, and copies otherwise. The
    //! disallowed overloads are removed from the overload set.
    //!
    //! @tparam U The target type (e.g. `Dog&&`, `const Dog&`, `Dog`).
    //! @param arg An rvalue reference to the `any` method argument.
    //! @return The value bound to `arg`, cast to `U`.
    template<
        typename U,
        typename = std::enable_if_t<
            (!std::is_rvalue_reference_v<U> || detail::te_owning<T>) &&
            (!detail::te_mutable_target<U> || detail::te_owning<T> ||
             detail::te_mutable_bound<T>)>>
    static auto cast(boost::type_erasure::any<C, T>&& arg) -> decltype(auto) {
        if constexpr (detail::te_pass_through<
                          U, boost::type_erasure::any<C, T>>) {
            return std::move(arg);
        } else if constexpr (std::is_rvalue_reference_v<U>) {
            return std::move(
                boost::type_erasure::any_cast<std::remove_reference_t<U>&>(
                    arg));
        } else if constexpr (!std::is_reference_v<U> && detail::te_owning<T>) {
            return U(std::move(boost::type_erasure::any_cast<U&>(arg)));
        } else {
            return boost::type_erasure::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for the mutable reference-wrapper flavor,
//! `boost::type_erasure::any<Concept, _self&>`, passed by value.
//!
//! The reference flavors are cheap, two-word handles; passing them by
//! value is the idiomatic way to use them as parameters. Dispatch is on
//! the type *bound at construction*, obtained via
//! `boost::type_erasure::typeid_of` - not the C++ RTTI dynamic type of
//! the referent. `Concept` must contain `boost::type_erasure::typeid_<>`;
//! `relaxed` implies it.
//!
//! @tparam C The `any`'s Concept.
//! @tparam T The referent placeholder (`_self` for `any<C, _self&>`).
//! @tparam Registry A @ref registry.
template<class C, typename T, class Registry>
struct virtual_traits<boost::type_erasure::any<C, T&>, Registry> {
    //! The type used for dispatch: the owning flavor for `C`.
    using virtual_type = boost::type_erasure::any<C>;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to an `any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::type_erasure::any<C, T&>& arg)
        -> const boost::type_erasure::any<C, T&>& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for the bound value.
    //!
    //! Looks up the @ref type_id returned by
    //! `boost::type_erasure::typeid_of` in the registry's `vptr` policy.
    //!
    //! @param arg A reference to a const `any`.
    //! @return A reference to the v-table pointer for the bound value.
    static auto
    vptr(const boost::type_erasure::any<C, T&>& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&boost::type_erasure::typeid_of(arg));
    }

    //! Cast to a type.
    //!
    //! Extracts the referent using `boost::type_erasure::any_cast`.
    //! Supports mutable references (e.g. `Dog&`); modifications through
    //! the result are visible through the referent. `U` cannot be an
    //! rvalue reference - the referent is borrowed, not owned; the
    //! overloads are removed from the overload set.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg The reference-wrapper `any` method argument.
    //! @return The value bound to `arg`, cast to `U`.
    template<
        typename U, typename = std::enable_if_t<!std::is_rvalue_reference_v<U>>>
    static auto cast(boost::type_erasure::any<C, T&> arg) -> decltype(auto) {
        if constexpr (detail::te_pass_through<
                          U, boost::type_erasure::any<C, T&>>) {
            // by value: a reference would dangle when this function's
            // parameter goes out of scope
            return arg;
        } else {
            return boost::type_erasure::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for the const reference-wrapper flavor,
//! `boost::type_erasure::any<Concept, const _self&>`, passed by value.
//!
//! The reference flavors are cheap, two-word handles; passing them by
//! value is the idiomatic way to use them as parameters. Dispatch is on
//! the type *bound at construction*, obtained via
//! `boost::type_erasure::typeid_of` - not the C++ RTTI dynamic type of
//! the referent. `Concept` must contain `boost::type_erasure::typeid_<>`;
//! `relaxed` implies it.
//!
//! @tparam C The `any`'s Concept.
//! @tparam T The referent placeholder (`_self` for
//! `any<C, const _self&>`).
//! @tparam Registry A @ref registry.
template<class C, typename T, class Registry>
struct virtual_traits<boost::type_erasure::any<C, const T&>, Registry> {
    //! The type used for dispatch: the owning flavor for `C`.
    using virtual_type = boost::type_erasure::any<C>;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to an `any`.
    //! @return A const reference to `arg`.
    static auto peek(const boost::type_erasure::any<C, const T&>& arg)
        -> const boost::type_erasure::any<C, const T&>& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for the bound value.
    //!
    //! Looks up the @ref type_id returned by
    //! `boost::type_erasure::typeid_of` in the registry's `vptr` policy.
    //!
    //! @param arg A reference to a const `any`.
    //! @return A reference to the v-table pointer for the bound value.
    static auto
    vptr(const boost::type_erasure::any<C, const T&>& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&boost::type_erasure::typeid_of(arg));
    }

    //! Cast to a type.
    //!
    //! Extracts the referent using `boost::type_erasure::any_cast`. Since
    //! the referent is const, `U` must be a value or a const reference;
    //! the other overloads are removed from the overload set.
    //!
    //! @tparam U The target type (e.g. `const Dog&`, `Dog`).
    //! @param arg The reference-wrapper `any` method argument.
    //! @return The value bound to `arg`, cast to `U`.
    template<
        typename U,
        typename = std::enable_if_t<
            !std::is_rvalue_reference_v<U> && !detail::te_mutable_target<U>>>
    static auto
    cast(boost::type_erasure::any<C, const T&> arg) -> decltype(auto) {
        if constexpr (detail::te_pass_through<
                          U, boost::type_erasure::any<C, const T&>>) {
            // by value: a reference would dangle when this function's
            // parameter goes out of scope
            return arg;
        } else {
            return boost::type_erasure::any_cast<U>(arg);
        }
    }
};

//! Register the types that a `boost::type_erasure::any` virtual parameter
//! may contain.
//!
//! Registers the owning flavor of `Any` (i.e.
//! `any<concept_of<Any>::type>`) as a class, and each `T` as a class
//! derived from it. This makes the bound types visible to the dispatch
//! machinery, which resolves a call on the `type_id` returned by
//! `boost::type_erasure::typeid_of`. `Any` may be spelled with any
//! flavor; the root is normalized to the owning flavor, which is also
//! what the virtual_traits use, whatever the flavor of the method
//! parameter.
//!
//! @tparam Any A `boost::type_erasure::any` type.
//! @tparam T... The types that may be bound to the `any`, optionally
//! followed by a @ref registry.
template<class Any, typename... T>
struct use_type_erasure_types
    : detail::use_any_types_aux<
          typename detail::extract_registry<T...>::registry,
          detail::type_erasure_root<Any>,
          typename detail::extract_registry<T...>::others> {};

// The primary final_virtual_ptr would silently use the static v-table
// pointer of the any class itself - the root -, not the bound value's.
// Delete the combination. Both call forms need covering: the (C, T)-only
// templates catch calls that deduce the default registry, and the
// Registry-first templates catch explicit-registry calls; both are more
// specialized than the primary's forwarding-reference parameter.

template<class C, typename T>
void final_virtual_ptr(const boost::type_erasure::any<C, T>&) = delete;
template<class C, typename T>
void final_virtual_ptr(boost::type_erasure::any<C, T>&) = delete;
template<class C, typename T>
void final_virtual_ptr(boost::type_erasure::any<C, T>&&) = delete;
template<class Registry, class C, typename T>
void final_virtual_ptr(const boost::type_erasure::any<C, T>&) = delete;
template<class Registry, class C, typename T>
void final_virtual_ptr(boost::type_erasure::any<C, T>&) = delete;
template<class Registry, class C, typename T>
void final_virtual_ptr(boost::type_erasure::any<C, T>&&) = delete;

namespace aliases {
using boost::openmethod::use_type_erasure_types;
} // namespace aliases

} // namespace boost::openmethod

#endif
