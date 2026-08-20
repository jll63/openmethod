// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_VIRTUAL_ANY_PTR_HPP
#define BOOST_OPENMETHOD_INTEROP_VIRTUAL_ANY_PTR_HPP

#include <boost/openmethod/interop/virtual_any.hpp>

#include <type_traits>
#include <utility>

namespace boost::openmethod {

BOOST_OPENMETHOD_OPEN_NAMESPACE_DETAIL_UNLESS_MRDOCS

//! Recognize a virtual_any_ptr (exposition only)
//!
//! The specialization of @ref IsVirtualAny that matches a
//! `virtual_any_ptr`, and evaluates to `true`.
//!
//! @tparam Any An `any` type, possibly const-qualified.
//! @tparam Registry A @ref registry.
template<class Any, class Registry>
constexpr bool IsVirtualAny<virtual_any_ptr<Any, Registry>> = true;

BOOST_OPENMETHOD_CLOSE_NAMESPACE_DETAIL_UNLESS_MRDOCS

//! A wide pointer to an `any`: a pointer to an `any`, and a pointer to
//! a v-table.
//!
//! `virtual_any_ptr` is the non-owning counterpart of @ref virtual_any:
//! it *points to* an existing `any` instead of holding a copy, and carries
//! the v-table pointer for the contained value, so methods dispatch on
//! the contained type without looking it up on every call. It is a
//! cheap, two-word handle with pointer semantics - copying it copies the
//! two words - and, like the reference-wrapper flavors of
//! Boost.TypeErasure's `any`, it is passed to methods *by value*.
//!
//! `Any` may be const-qualified: through `virtual_any_ptr<const Any>`,
//! overriders can only take the contained value by value or by const
//! reference; `virtual_any_ptr<Any>` also supports mutable references,
//! and modifications reach the referent. A `virtual_any_ptr<Any>`
//! converts to a `virtual_any_ptr<const Any>`. The `any` itself is
//! reached read-only either way - see @ref get.
//!
//! The v-table pointer is acquired when the handle is created: from the
//! dynamic type of the value contained in the `any` (a hash table
//! lookup), or at no cost from a @ref virtual_any, which already carries
//! it. Assigning re-derives it, so a handle can be pointed at another
//! `any`, and @ref emplace can replace the value inside the one it points
//! to. The handle does not track its referent otherwise: if the value is
//! replaced behind its back - through the `any` itself, the @ref
//! virtual_any that owns it, or another handle - it is stale, like an
//! iterator into a modified container, and must be re-created.
//!
//! An overrider takes the *contained* value - or, for a catch-all
//! overrider, the `virtual_any_ptr` itself, by value. Since a plain
//! value does not convert to a `virtual_any_ptr`, overriders taking the
//! contained value are registered with the core API
//! (`method<...>::override<fn>`) rather than with
//! @ref BOOST_OPENMETHOD_OVERRIDE, which locates the method by
//! convertibility.
//!
//! `Any` can be `std::any`, `boost::any`, or any type that has an
//! `any`-like interface, and specializes `virtual_traits` for its
//! reference types, providing `vptr` and `cast`.
//!
//! @tparam Any An `any` type, possibly const-qualified.
//! @tparam Registry A @ref registry.
//!
//! @par Example
//! include:virtual_any.cpp#ptr;ptr_dispatch
//!
//! @see [Interoperation with `any`](xref:ROOT:interop_any.adoc)
template<class Any, class Registry>
class virtual_any_ptr {
    static constexpr bool use_indirect_vptrs = Registry::has_indirect_vptr;

    using owner_type = std::conditional_t<
        std::is_const_v<Any>,
        const virtual_any<std::remove_const_t<Any>, Registry>,
        virtual_any<std::remove_const_t<Any>, Registry>>;

    Any* obj;
    std::conditional_t<use_indirect_vptrs, const vptr_type*, vptr_type> vp;

    template<typename, class>
    friend struct virtual_traits;

    template<class, class>
    friend class virtual_any_ptr;

  public:
    //! The type of the `any` pointed to.
    //!
    //! Always const-qualified: the `any` is not modifiable through the
    //! handle. `Any` itself may or may not be const; that governs what
    //! overriders may take, not this.
    using element_type = const Any;

    //! Construct from an `any`.
    //!
    //! Acquires the v-table pointer for the contained value, using
    //! `virtual_traits<const Any&, Registry>::vptr` - a hash table
    //! lookup.
    //!
    //! @param other An `any` lvalue.
    virtual_any_ptr(Any& other)
        : obj(&other), vp(detail::box_vptr<use_indirect_vptrs>(
                           detail::acquire_vptr<Registry>(other))) {
    }

    //! A `virtual_any_ptr` cannot point to a temporary `any`.
    virtual_any_ptr(std::remove_const_t<Any>&&) = delete;

    //! Construct from a `virtual_any`.
    //!
    //! Points to the `any` held by `other`, and copies its v-table
    //! pointer - no lookup is involved. A `virtual_any_ptr<const Any>` can
    //! point to a const `virtual_any`; a mutable one requires a
    //! mutable `virtual_any`.
    //!
    //! @param other A `virtual_any` lvalue.
    virtual_any_ptr(owner_type& other) : obj(&other.obj), vp(other.vp) {
    }

    //! A `virtual_any_ptr` cannot point to a temporary `virtual_any`.
    virtual_any_ptr(std::remove_const_t<owner_type>&&) = delete;

    //! Convert a mutable `virtual_any_ptr` to a const one.
    template<
        class Other,
        typename = std::enable_if_t<
            std::is_const_v<Any> &&
            std::is_same_v<Other, std::remove_const_t<Any>>>>
    virtual_any_ptr(virtual_any_ptr<Other, Registry> other)
        : obj(other.obj), vp(other.vp) {
    }

    //! Point to another `any`.
    //!
    //! Acquires the v-table pointer for the value contained in `other`,
    //! using `virtual_traits<const Any&, Registry>::vptr` - a hash table
    //! lookup.
    //!
    //! @param other An `any` lvalue.
    auto operator=(Any& other) -> virtual_any_ptr& {
        obj = &other;
        vp = detail::box_vptr<use_indirect_vptrs>(
            detail::acquire_vptr<Registry>(other));
        return *this;
    }

    //! A `virtual_any_ptr` cannot point to a temporary `any`.
    auto operator=(std::remove_const_t<Any>&&) -> virtual_any_ptr& = delete;

    //! Point to the `any` held by a `virtual_any`.
    //!
    //! Copies `other`'s v-table pointer - no lookup is involved.
    //!
    //! @param other A `virtual_any` lvalue.
    auto operator=(owner_type& other) -> virtual_any_ptr& {
        obj = &other.obj;
        vp = other.vp;
        return *this;
    }

    //! A `virtual_any_ptr` cannot point to a temporary `virtual_any`.
    auto
    operator=(std::remove_const_t<owner_type>&&) -> virtual_any_ptr& = delete;

    //! Assign a mutable `virtual_any_ptr` to a const one.
    template<
        class Other,
        typename = std::enable_if_t<
            std::is_const_v<Any> &&
            std::is_same_v<Other, std::remove_const_t<Any>>>>
    auto operator=(virtual_any_ptr<Other, Registry> other) -> virtual_any_ptr& {
        obj = other.obj;
        vp = other.vp;
        return *this;
    }

    //! Construct a value in place in the `any`.
    //!
    //! Stores a `Class` constructed from `args` in the `any` this handle
    //! points to, and sets the v-table pointer to the
    //! @ref registry::static_vptr for `Class` - no hash table lookup is
    //! involved. This is the only way to replace the contained value
    //! through the handle, and it maintains the invariant that the
    //! v-table pointer corresponds to the contained type.
    //!
    //! Other handles to the same `any`, and the @ref virtual_any that
    //! owns it, if any, are left stale.
    //!
    //! @tparam Class The type of the value to construct.
    //! @tparam T Types of the arguments to pass to the constructor.
    //! @param args Arguments to pass to the constructor of `Class`.
    template<class Class, typename... T>
    auto emplace(T&&... args) -> void {
        static_assert(
            !std::is_const_v<Any>,
            "cannot modify the `any` through a virtual_any_ptr<const Any>");
        *obj = Class(std::forward<T>(args)...);
        Registry::require_initialized();
        vp = detail::box_vptr<use_indirect_vptrs>(
            Registry::template static_vptr<Class>);
        BOOST_ASSERT(detail::unbox_vptr(vp) != nullptr);
    }

    //! Return a pointer to the (non-modifiable) `any`.
    //!
    //! The `any` is not modifiable through the handle, whether or not
    //! `Any` is const-qualified: replacing the value it contains would
    //! leave the handle carrying the v-table pointer of the previous
    //! value. Use @ref emplace, which re-derives it. The contained value
    //! itself is modifiable, through an overrider taking a mutable
    //! reference, which a `virtual_any_ptr<const Any>` rules out.
    //!
    //! @return A pointer to the `any`.
    auto get() const -> element_type* {
        return obj;
    }

    //! Return a pointer to the (non-modifiable) `any`.
    //!
    //! Same as @ref get.
    //!
    //! @return A pointer to the `any`.
    auto operator->() const -> element_type* {
        return obj;
    }

    //! Return a reference to the (non-modifiable) `any`.
    //!
    //! Same as `*get()`.
    //!
    //! @return A reference to the `any`.
    auto operator*() const -> element_type& {
        return *obj;
    }

    //! Return a pointer to the (non-modifiable) `any`.
    //!
    //! Same as @ref get. Provided for symmetry with @ref virtual_ptr,
    //! where `pointer` returns the stored pointer object, which differs
    //! from `get` for the smart pointer specialization.
    //!
    //! @return A pointer to the `any`.
    auto pointer() const -> element_type* {
        return obj;
    }

    //! Return the v-table pointer.
    auto vptr() const -> vptr_type {
        return detail::unbox_vptr(vp);
    }

#ifndef __MRDOCS__
    // Constrained to exactly this `virtual_any_ptr`, for the same reason
    // as in `virtual_any`: MSVC, in its default (permissive) mode,
    // injects friend functions into the enclosing namespace, where an
    // unconstrained parameter would make this a candidate for anything
    // convertible to `virtual_any_ptr`.
    template<class Self>
    friend auto boost_openmethod_vptr(const Self& va, Registry*)
        -> std::enable_if_t<std::is_same_v<Self, virtual_any_ptr>, vptr_type> {
        return detail::unbox_vptr(va.vp);
    }
#endif
};

//! Construct a `virtual_any_ptr` from an `any` lvalue.
//!
//! The `any`'s const-qualification is deduced along with its type, so a
//! const `any` yields a `virtual_any_ptr<const Any>`.
//!
//! @tparam Any An `any` type, possibly const-qualified.
//! @param other An `any` lvalue.
//! @return A `virtual_any_ptr<Any>`.
template<
    class Any,
    typename = std::enable_if_t<!BOOST_OPENMETHOD_DETAIL_UNLESS_MRDOCS
                                    IsVirtualAny<std::remove_const_t<Any>>>>
virtual_any_ptr(Any& other)
    -> virtual_any_ptr<Any, BOOST_OPENMETHOD_DEFAULT_REGISTRY>;

//! Construct a `virtual_any_ptr` from a `virtual_any` lvalue.
//!
//! Deduces the registry from `other`.
//!
//! @tparam Any An `any` type.
//! @tparam Registry A @ref registry.
//! @param other A `virtual_any` lvalue.
//! @return A `virtual_any_ptr<Any, Registry>`.
template<class Any, class Registry>
virtual_any_ptr(virtual_any<Any, Registry>& other)
    -> virtual_any_ptr<Any, Registry>;

//! Construct a `virtual_any_ptr` from a const `virtual_any` lvalue.
//!
//! Deduces the registry from `other`, and yields a const handle.
//!
//! @tparam Any An `any` type.
//! @tparam Registry A @ref registry.
//! @param other A const `virtual_any` lvalue.
//! @return A `virtual_any_ptr<const Any, Registry>`.
template<class Any, class Registry>
virtual_any_ptr(const virtual_any<Any, Registry>& other)
    -> virtual_any_ptr<const Any, Registry>;

//! Compare two `virtual_any_ptr`s for equality.
//!
//! Compare the pointers to the `any`s for equality. The v-table pointers
//! are not compared.
//!
//! @tparam Left The `any` type of the left-hand side argument.
//! @tparam Right The `any` type of the right-hand side argument.
//! @tparam Registry A @ref registry.
//! @param left A `virtual_any_ptr`.
//! @param right A `virtual_any_ptr`.
//! @return `true` if both `virtual_any_ptr`s point to the same `any`,
//! `false` otherwise.
template<class Left, class Right, class Registry>
auto operator==(
    const virtual_any_ptr<Left, Registry>& left,
    const virtual_any_ptr<Right, Registry>& right) -> bool {
    return left.pointer() == right.pointer();
}

//! Compare two `virtual_any_ptr`s for inequality.
//!
//! Compare the pointers to the `any`s for inequality. The v-table
//! pointers are not compared.
//!
//! @tparam Left The `any` type of the left-hand side argument.
//! @tparam Right The `any` type of the right-hand side argument.
//! @tparam Registry A @ref registry.
//! @param left A `virtual_any_ptr`.
//! @param right A `virtual_any_ptr`.
//! @return `true` if the `virtual_any_ptr`s point to different `any`s,
//! `false` otherwise.
template<class Left, class Right, class Registry>
auto operator!=(
    const virtual_any_ptr<Left, Registry>& left,
    const virtual_any_ptr<Right, Registry>& right) -> bool {
    return !(left == right);
}

//! Specialize virtual_traits for `virtual_any_ptr`, passed by value.
//!
//! Dispatch is on the v-table pointer stored in the `virtual_any_ptr`.
//!
//! @tparam Any An `any` type, possibly const-qualified.
//! @tparam Registry A @ref registry.
template<class Any, class Registry>
struct virtual_traits<virtual_any_ptr<Any, Registry>, Registry> {
    //! The type used for dispatch.
    using virtual_type = std::remove_const_t<Any>;

    //! Returns a const reference to the `virtual_any_ptr` argument.
    //! @param arg A reference to a `virtual_any_ptr`.
    //! @return A const reference to `arg`.
    static auto peek(const virtual_any_ptr<Any, Registry>& arg)
        -> const virtual_any_ptr<Any, Registry>& {
        return arg;
    }

    //! Cast to a type.
    //!
    //! If `U` is the `virtual_any_ptr` itself, returns a copy of the
    //! handle. Otherwise, extracts the pointee's value using the
    //! `virtual_traits` for the `any`'s reference type: mutable
    //! references (e.g. `Dog&`) are supported unless `Any` is
    //! const-qualified.
    //!
    //! @tparam U The target type (e.g. `Dog&`, `const Dog&`, `Dog`).
    //! @param arg The `virtual_any_ptr` method argument.
    //! @return The value pointed to by `arg`, cast to `U`.
    template<typename U>
    static auto cast(virtual_any_ptr<Any, Registry> arg) -> decltype(auto) {
        if constexpr (std::is_same_v<
                          std::remove_cv_t<std::remove_reference_t<U>>,
                          virtual_any_ptr<Any, Registry>>) {
            // by value: a reference would dangle when this function's
            // parameter goes out of scope
            return arg;
        } else {
            // `Any&` is `const virtual_type&` when `Any` is
            // const-qualified, and `virtual_type&` otherwise - so a single
            // specialization covers both, and mutable references are
            // available exactly when `Any` is not const. This reads the
            // private `obj`, not the const-only public accessors, because
            // that is precisely what a mutable `U` needs.
            return virtual_traits<Any&, Registry>::template cast<U>(*arg.obj);
        }
    }
};

namespace detail {

template<class Any, class Registry>
struct is_virtual<virtual_any_ptr<Any, Registry>> : std::true_type {};

template<class Any, class Registry>
struct parameter_traits<virtual_any_ptr<Any, Registry>, Registry>
    : virtual_traits<virtual_any_ptr<Any, Registry>, Registry> {};

template<class Any, class Registry, class MethodRegistry>
struct validate_method_parameter<
    virtual_any_ptr<Any, Registry>, MethodRegistry, void>
    : std::bool_constant<std::is_same_v<Registry, MethodRegistry>> {
    static_assert(
        std::is_same_v<Registry, MethodRegistry>, "registry mismatch");
};

template<class Any, class Registry, class MethodRegistry>
struct validate_method_parameter<
    virtual_any_ptr<Any, Registry>&, MethodRegistry, void> : std::false_type {
    static_assert(
        false_t<Any, Registry>,
        "virtual_any_ptr is a cheap handle, pass it by value");
};

template<class Any, class Registry, class MethodRegistry>
struct validate_method_parameter<
    const virtual_any_ptr<Any, Registry>&, MethodRegistry, void>
    : std::false_type {
    static_assert(
        false_t<Any, Registry>,
        "virtual_any_ptr is a cheap handle, pass it by value");
};

template<class Any, class Registry, class MethodRegistry>
struct validate_method_parameter<
    virtual_any_ptr<Any, Registry>&&, MethodRegistry, void> : std::false_type {
    static_assert(
        false_t<Any, Registry>,
        "virtual_any_ptr is a cheap handle, pass it by value");
};

template<class Any, class Registry, typename T2>
struct validate_overrider_parameter<virtual_any_ptr<Any, Registry>, T2, void>
    : std::true_type {};

template<class Any, class Registry>
struct validate_overrider_parameter<
    virtual_any_ptr<Any, Registry>, virtual_any_ptr<Any, Registry>, void>
    : std::true_type {};

template<class Any, class Registry, typename Q>
struct select_overrider_virtual_type_aux<
    virtual_any_ptr<Any, Registry>, Q, Registry> {
    using type = virtual_type<Q, Registry>;
};

} // namespace detail

namespace aliases {
using boost::openmethod::virtual_any_ptr;
} // namespace aliases

} // namespace boost::openmethod

#endif
