// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_MINIMAL_RTTI_HPP
#define BOOST_OPENMETHOD_POLICY_MINIMAL_RTTI_HPP

#include <boost/openmethod/preamble.hpp>

#include <boost/current_function.hpp>

#include <cstddef>
#include <functional>
#include <string_view>
#include <type_traits>

namespace boost::openmethod::policies {

//! Minimal implementation of the `rtti` policy.
//!
//! `static_rtti` implements only the static parts of the `rtti` policy. It uses
//! the addresses of a per-class static variables as a `type_id`. It categorizes
//! all types as non-polymorphic. As a consequence, the `virtual_ptr`
//! constructors that take a pointer or a reference to a polymorphic object are
//! disabled. `virtual_ptr` must be constructed using @ref final_virtual_ptr (or
//! its equivalents for smart pointers).
//!
//! Each module of a program has its own copy of those variables, unless the
//! dynamic linker merges them, which it does not do on Windows, nor for symbols
//! with hidden visibility. A class used by several modules then has one
//! `type_id` per module, and the registry takes the copies for unrelated
//! classes. Use @ref named_static_rtti in a program that shares a registry
//! across modules.
//!
//! @par Example
//!
//! Selecting the policy. The registry is declared before
//! `<boost/openmethod/core.hpp>` (or any header that includes it, like
//! `<boost/openmethod.hpp>`), and defined after it:
//!
//! include:static_rtti.cpp#registry
//!
//! The classes and the method need no RTTI, and need not be polymorphic:
//!
//! include:static_rtti.cpp#classes
//!
//! Every `virtual_ptr` has to be created where the exact class is known:
//!
//! include:static_rtti.cpp#dispatch
//!
//! @see [Custom RTTI](xref:ROOT:custom_rtti.adoc)
struct static_rtti : rtti {
    //! A RttiFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy.
    template<class Registry>
    struct fn : rtti::defaults {
        //! Always evaluates to `false`.
        //! @tparam Class A class.
        template<class Class>
        static constexpr bool is_polymorphic = false;

        //! Returns the @ref type_id of `Class`.
        //!
        //! @tparam Class A class.
        template<typename T>
        static auto static_type() -> type_id {
            static char id;

            return &id;
        }
    };
};

} // namespace boost::openmethod::policies

namespace boost::openmethod::detail {

// The signature of this function, as the compiler prints it, contains the name
// of `T`. Its prefix and suffix are the same for every `T`; they are measured
// on `void`.
template<typename T>
constexpr auto static_rtti_signature() -> const char* {
    return BOOST_CURRENT_FUNCTION;
}

inline constexpr std::string_view static_rtti_probe =
    static_rtti_signature<void>();
inline constexpr std::size_t static_rtti_prefix =
    static_rtti_probe.find("void");

template<typename T>
constexpr auto static_rtti_type_name() -> std::string_view {
    // Without __PRETTY_FUNCTION__ or __FUNCSIG__, BOOST_CURRENT_FUNCTION falls
    // back on a name that does not contain `T`, and every class would get the
    // same name.
    static_assert(
        static_rtti_prefix != std::string_view::npos,
        "named_static_rtti requires __PRETTY_FUNCTION__ or __FUNCSIG__");

    constexpr std::size_t suffix =
        static_rtti_probe.size() - static_rtti_prefix - 4;
    constexpr std::string_view signature = static_rtti_signature<T>();

    return signature.substr(
        static_rtti_prefix, signature.size() - static_rtti_prefix - suffix);
}

constexpr auto static_rtti_is_qualifier(char c) -> bool {
    return (c >= 'a' && c <= 'z') || c == ' ' || c == '&';
}

// A class with internal linkage, or none, may print the same name as a
// different class. Only the compilers know which classes they are; this guesses
// it from the marks they leave in the name. GCC prints a local class as
// `f()::L`, or `S::f() const::L`: a scope closed by a parenthesis, maybe
// followed by qualifiers.
constexpr auto static_rtti_marked_local(std::string_view name) -> bool {
    constexpr std::string_view marks[] = {
        "{anonymous}",           // GCC
        "(anonymous namespace)", // Clang
        "`",           // MSVC: `anonymous-namespace', `anonymous namespace'
        "<lambda",     // GCC, MSVC
        "(lambda at ", // Clang
        "<unnamed",    // GCC, MSVC
        "(unnamed ",   // Clang
    };

    for (auto mark : marks) {
        if (name.find(mark) != std::string_view::npos) {
            return true;
        }
    }

    for (
        auto pos = name.find("::"); pos != std::string_view::npos;
        pos = name.find("::", pos + 2)) {
        auto i = pos;

        while (i > 0 && static_rtti_is_qualifier(name[i - 1])) {
            --i;
        }

        if (i > 0 && name[i - 1] == ')') {
            return true;
        }
    }

    return false;
}

// Whether the copies of `T`'s type_id in different modules designate the same
// type, and can be merged by name.
template<typename T>
constexpr auto static_rtti_shareable() -> bool {
#if BOOST_OPENMETHOD_HAS_REFLECTION
    if constexpr (
        std::is_class_v<T> || std::is_union_v<T> || std::is_enum_v<T>) {
        return std::meta::has_external_linkage(^^T);
    } else {
        return true;
    }
#else
    return !static_rtti_marked_local(static_rtti_type_name<T>());
#endif
}

// What a named_static_rtti type_id points to.
struct static_rtti_name {
    std::string_view name;
    bool shareable;
};

// What named_static_rtti::type_index returns: the name, and for a class that
// cannot be shared, the type_id as well, which keeps it apart from other
// classes with the same name.
struct static_rtti_key {
    std::string_view name;
    const void* local;

    friend auto operator==(const static_rtti_key& a, const static_rtti_key& b)
        -> bool {
        return a.local == b.local && a.name == b.name;
    }

    friend auto operator!=(const static_rtti_key& a, const static_rtti_key& b)
        -> bool {
        return !(a == b);
    }

    friend auto operator<(const static_rtti_key& a, const static_rtti_key& b)
        -> bool {
        if (a.name != b.name) {
            return a.name < b.name;
        }

        return std::less<const void*>()(a.local, b.local);
    }
};

} // namespace boost::openmethod::detail

template<>
struct std::hash<boost::openmethod::detail::static_rtti_key> {
    auto operator()(const boost::openmethod::detail::static_rtti_key& key) const
        -> std::size_t {
        return std::hash<std::string_view>()(key.name) ^
            std::hash<const void*>()(key.local);
    }
};

namespace boost::openmethod::policies {

//! Implementation of the `rtti` policy that identifies classes by name.
//!
//! `named_static_rtti` is a @ref static_rtti that works across modules. It
//! categorizes all types as non-polymorphic, and its type ids are addresses
//! that differ from one module to the next, like those of @ref static_rtti. In
//! addition, `type_index` maps them to the name of the class, as the compiler
//! prints it, so the registry merges the copies of a class coming from
//! different modules.
//!
//! All the modules must be built with the same compiler, because the names
//! must be spelled the same way. The cost is paid once, by @ref initialize.
//! Dispatch does not use names.
//!
//! A class that has internal linkage, or none, is not shared: it is identified
//! by its `type_id`, as with @ref static_rtti. This covers the classes defined
//! in an anonymous namespace, local classes, lambdas, unnamed classes, and the
//! templates instantiated with any of these. With C++26 reflection, the policy
//! asks the compiler which classes they are. Otherwise, it recognizes them from
//! the marks that the compiler leaves in their names.
//!
//! @warning Clang and MSVC leave no mark in the name of a local class. Clang
//! prints it unqualified, and MSVC prints it as if the function were a
//! namespace (`main::Local`). Without reflection, two local classes that print
//! the same name are taken for the same class, and so is a local class that
//! prints the same name as a class at namespace scope. So is a class template
//! instantiated with an entity that has internal linkage, such as the address
//! of a `static` variable, with all compilers.
//!
//! @see [Custom RTTI](xref:ROOT:custom_rtti.adoc)
struct named_static_rtti : rtti {
    //! A RttiFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy.
    template<class Registry>
    struct fn : static_rtti::fn<Registry> {
        //! Returns the @ref type_id of `Class`.
        //!
        //! @tparam Class A class.
        template<typename T>
        static auto static_type() -> type_id {
            // Not const: two local classes can have the same name, and a
            // linker may fold identical read-only objects into one.
            static detail::static_rtti_name id{
                detail::static_rtti_type_name<T>(),
                detail::static_rtti_shareable<T>()
            };

            return &id;
        }

        //! Returns a key that identifies a class across modules.
        //!
        //! @param type A `type_id`.
        //! @return The name of the class, and `type` itself if the class
        //! cannot be shared.
        static auto type_index(type_id type) -> detail::static_rtti_key {
            auto id = static_cast<const detail::static_rtti_name*>(type);

            return {id->name, id->shareable ? nullptr : type};
        }

        //! Writes the name of a class to a stream.
        //!
        //! @param type A `type_id`.
        //! @param stream A stream to write to.
        template<typename Stream>
        static void type_name(type_id type, Stream& stream) {
            stream << static_cast<const detail::static_rtti_name*>(type)->name;
        }
    };
};

} // namespace boost::openmethod::policies

#endif
