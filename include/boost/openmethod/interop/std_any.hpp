// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_STD_ANY_HPP
#define BOOST_OPENMETHOD_INTEROP_STD_ANY_HPP

#include <any>
#include <boost/openmethod/core.hpp>

namespace boost::openmethod {

//! Specialize virtual_traits for std::any by value.
//!
//! Dispatch is based on the runtime type of the value stored in the `any`,
//! obtained via `std::any::type()`. Requires the registry to use a @ref
//! rtti policy that provides `dynamic_type` (e.g. @ref std_rtti).
//!
//! @tparam Registry A @ref registry.
template<class Registry>
struct virtual_traits<std::any, Registry> {
    //! The type used for dispatch.
    using virtual_type = std::any;

    //! Returns a const reference to the `any` argument.
    //! @param arg A reference to a `std::any`.
    //! @return A const reference to `arg`.
    static auto peek(const std::any& arg) -> const std::any& {
        return arg;
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
        if constexpr (std::is_same_v<std::remove_cvref_t<U>, std::any>) {
            return std::move(arg);
        } else {
            return std::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for `std::any&` (mutable reference).
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

    //! Cast to a type.
    //!
    //! Extracts the stored value using `std::any_cast`. Supports mutable
    //! references (e.g. `Dog&`) because the `any` argument is non-const.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg A mutable reference to the `std::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<typename U>
    static auto cast(std::any& arg) -> decltype(auto) {
        if constexpr (std::is_same_v<std::remove_cvref_t<U>, std::any>) {
            return (std::any&)arg;
        } else {
            return std::any_cast<U>(arg);
        }
    }
};

//! Specialize virtual_traits for `const std::any&` (read-only reference).
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

    //! Cast to a type.
    //!
    //! Extracts the stored value using `std::any_cast`.
    //!
    //! @tparam U The target type (e.g. `const Dog&`, `Dog`).
    //! @param arg A const reference to the `std::any` method argument.
    //! @return The value stored in `arg`, cast to `U`.
    template<typename U>
    static auto cast(const std::any& arg) -> decltype(auto) {
        if constexpr (std::is_same_v<std::remove_cvref_t<U>, std::any>) {
            return (const std::any&)arg;
        } else {
            return std::any_cast<U>(arg);
        }
    }
};

//! Acquire a v-table pointer from a `std::any`.
//!
//! Uses `std::any::type()` to obtain the @ref type_id of the stored value
//! and looks it up in the registry's v-table store. Only participates in
//! overload resolution for registries whose @ref rtti policy provides
//! `dynamic_type` (e.g. @ref std_rtti).
//!
//! @tparam Registry A @ref registry.
//! @param arg A const reference to a `std::any`.
//! @return The v-table pointer for the stored value's type.
template<
    class Registry,
    class = std::void_t<decltype(
        Registry::rtti::dynamic_type(std::declval<const int&>()))>>
auto boost_openmethod_vptr(const std::any& arg, Registry*) -> vptr_type {
    return Registry::template policy<policies::vptr>::dynamic_vptr(
        &arg.type());
}

} // namespace boost::openmethod

#endif
