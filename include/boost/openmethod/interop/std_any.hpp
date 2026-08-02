// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_STD_ANY_HPP
#define BOOST_OPENMETHOD_INTEROP_STD_ANY_HPP

#include <any>
#include <boost/openmethod/core.hpp>

namespace boost::openmethod {

namespace detail {

template<class Registry>
struct validate_method_parameter<virtual_<const std::any&>, Registry, void>
    : std::true_type {};

template<class Registry>
struct validate_method_parameter<virtual_<std::any&>, Registry, void>
    : std::true_type {};

template<class Registry>
struct validate_method_parameter<virtual_<std::any&&>, Registry, void>
    : std::true_type {};

} // namespace detail

//! Specialize virtual_traits for `const std::any&` (const reference).
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `std::any::type()`.
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<const std::any&, Registry> {
    //! The type used for dispatch.
    using virtual_type = std::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `std::any`.
    //! @return A const reference to `arg`.
    static auto peek(const std::any& arg) -> const std::any& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for an object.
    //!
    //! Acquires the dynamic @ref type_id of `arg`, using the registry's
    //! @ref rtti policy.
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
    static auto dynamic_vptr(const std::any& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&arg.type());
    }

    //! Cast to a type.
    //!
    //! Extracts the stored value using `std::any_cast`. Since the `any`
    //! argument is const, `U` cannot be a mutable reference.
    //!
    //! @tparam U The target type (e.g. `const Dog&`, `Dog`).
    //! @param arg A reference to a const `std::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<typename U>
    static auto cast(const std::any& arg) -> decltype(auto) {
        return std::any_cast<U>(arg);
    }
};

//! Specialize virtual_traits for `std::any&` (mutable reference).
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `std::any::type()`.
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<std::any&, Registry> {
    //! The type used for dispatch.
    using virtual_type = std::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `std::any`.
    //! @return A const reference to `arg`.
    static auto peek(const std::any& arg) -> const std::any& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for an object.
    //!
    //! Acquires the @ref type_id of the value stored in `arg`, using
    //! `std::any::type()`.
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
    //! @param arg A reference to a `std::any`.
    //! @return A reference to the v-table pointer for the stored value.
    static auto dynamic_vptr(const std::any& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&arg.type());
    }

    //! Cast to a type.
    //!
    //! Extracts the stored value using `std::any_cast`. Supports mutable
    //! references (e.g. `Dog&`) because the `any` argument is not const;
    //! modifications through the result are visible through the `any`.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg A mutable reference to the `std::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<typename U>
    static auto cast(std::any& arg) -> decltype(auto) {
        return std::any_cast<U>(arg);
    }
};

//! Specialize virtual_traits for `std::any&&` (xvalue reference).
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `std::any::type()`.
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<std::any&&, Registry> {
    //! The type used for dispatch.
    using virtual_type = std::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `std::any`.
    //! @return A const reference to `arg`.
    static auto peek(const std::any& arg) -> const std::any& {
        return arg;
    }

    //! Returns a *reference* to a v-table pointer for an object.
    //!
    //! Acquires the dynamic @ref type_id of `arg`, using the registry's
    //! @ref rtti policy.
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
    //! @return A reference to a the v-table pointer for `Class`.
    static auto dynamic_vptr(const std::any& arg) -> const vptr_type& {
        return Registry::vptr::vptr(&arg.type());
    }

    //! Cast to a type.
    //!
    //! Extracts the stored value using `std::any_cast`.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg An rvalue reference to the `std::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<typename U>
    static auto cast(std::any&& arg) -> decltype(auto) {
        return std::any_cast<U>(std::move(arg));
    }
};

//! Register the types that a `std::any` virtual parameter may contain.
//!
//! Registers `std::any` as a class, and each `T` as a class derived from
//! `std::any`. This makes the contained types visible to the dispatch
//! machinery, which resolves a call on the `type_id` returned by
//! `std::any::type()`.
//!
//! @tparam T... The types that may be stored in the `any`, optionally
//! followed by a @ref registry.
template<typename... T>
struct use_std_any_types
    : detail::use_class_aux<
          typename detail::extract_registry<T...>::registry,
          mp11::mp_list<std::any, std::any>>,
      detail::use_class_aux<
          typename detail::extract_registry<T...>::registry,
          mp11::mp_list<T, std::any>>... {};

} // namespace boost::openmethod

#endif
