// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_STD_RTTI_HPP
#define BOOST_OPENMETHOD_POLICY_STD_RTTI_HPP

#include <boost/openmethod/preamble.hpp>
#include <boost/openmethod/detail/type_key.hpp>

#include <cstddef>
#include <string_view>

#ifndef BOOST_NO_RTTI
#include <typeindex>
#include <typeinfo>
#include <boost/core/demangle.hpp>
#endif

// Whether std::type_index identifies classes the way initialize() needs: equal
// for the copies of one class in different modules, and different for two
// classes with internal linkage that have the same name. See
// std_rtti::type_index.
#ifndef BOOST_OPENMETHOD_DETAIL_STD_TYPE_INDEX
#if defined(_MSC_VER) ||                                                       \
    (defined(__GLIBCXX__) && defined(__GNUC__) && !defined(__clang__) &&       \
        !defined(__INTEL_COMPILER))
#define BOOST_OPENMETHOD_DETAIL_STD_TYPE_INDEX 1
#else
#define BOOST_OPENMETHOD_DETAIL_STD_TYPE_INDEX 0
#endif
#endif

namespace boost::openmethod::detail {

inline auto mangled_is_digit(char c) -> bool {
    return c >= '0' && c <= '9';
}

// Whether a type name mangled per the Itanium C++ ABI designates a type that a
// different type, in another translation unit, may share the name of: one with
// internal linkage, or none. The mangling says so:
//
//   _GLOBAL__N...   begins the name of an anonymous namespace
//   L <name>        at the start of a name, or in the encoding of a function
//                   or variable (after `Z`, or `_Z` in an expression), names a
//                   static one: a type local to a static function, a lambda
//                   in the initializer of a static variable, or a template
//                   argument that is the address of a static variable
//   Ut              an unnamed type
//   $ or .          in a name, Clang's or GCC's name for an unnamed type, or a
//                   lambda
//
// Source names are skipped by their length, so an identifier like `ZLib` is
// not taken for a mark, and so are the numbers that are not lengths. A mark
// read where there is none only keeps the copies of a class apart, as if it
// had internal linkage.
inline auto itanium_internal_linkage(std::string_view name) -> bool {
    const std::size_t n = name.size();
    std::size_t i = 0;
    bool encoding = false;
    bool literal_value = false;

    auto skip_number = [&]() {
        if (i < n && name[i] == 'n') {
            ++i;
        }

        while (i < n && mangled_is_digit(name[i])) {
            ++i;
        }
    };

    while (i < n) {
        char c = name[i];
        char next = i + 1 < n ? name[i + 1] : '\0';

        if (mangled_is_digit(c)) {
            std::size_t length = 0;

            while (i < n && mangled_is_digit(name[i])) {
                length = 10 * length + std::size_t(name[i++] - '0');
            }

            auto identifier = name.substr(i, length);

            // An anonymous namespace, or a name that is not an identifier:
            // `$_0` (Clang) or `._anon_0` (GCC) for an unnamed type.
            if (identifier.substr(0, 10) == "_GLOBAL__N" ||
                identifier.find_first_of("$.") != std::string_view::npos) {
                return true;
            }

            i += length;

            // The value of an enum literal follows its type.
            if (literal_value) {
                literal_value = false;
                skip_number();
            }

            continue;
        }

        switch (c) {
        case '$':
            return true;

        case 'U':
            if (next == 't') {
                return true;
            }

            break;

        case 'Z':
            encoding = true;
            break;

        case 'L':
            if (mangled_is_digit(next)) {
                // A static function or variable, in an encoding or at the
                // start of a name; an enum literal never comes there.
                if (encoding || i == 0 || name[i - 1] == 'N') {
                    return true;
                }

                // An enum literal: L <type> <value> E.
                literal_value = true;
            } else if (next >= 'a' && next <= 'z') {
                // A literal of a builtin type: L <type> <value> E.
                i += 2;
                skip_number();
                continue;
            }

            break;

        case 'S':
            // A substitution, S <base 36 number> _, or an abbreviation like St.
            ++i;

            while (
                i < n &&
                (mangled_is_digit(name[i]) ||
                    (name[i] >= 'A' && name[i] <= 'Z'))) {
                ++i;
            }

            continue;

        case 'D':
            // A vector type: Dv <number> _.
            if (next == 'v') {
                i += 2;
                skip_number();
                continue;
            }

            break;

        case 'T': // a template parameter: T <number> _
        case 'A': // an array type: A <number> _
        case '_': // a discriminator: _ <digit>, or __ <number> _
            ++i;
            skip_number();
            continue;
        }

        ++i;
    }

    return false;
}

} // namespace boost::openmethod::detail

namespace boost::openmethod::policies {

//! Implements the @ref rtti policy using standard RTTI.
//!
//! `std_rtti` implements the `rtti` policy using the standard C++ RTTI system.
//! It is the default RTTI policy.
//!
//! @par Example
//! include:policies.cpp#std_rtti;std_rtti_dispatch
//!
//! @see [Custom RTTI](xref:ROOT:custom_rtti.adoc)
struct std_rtti : rtti {
    //! A RttiFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy.
    template<class Registry>
    struct fn {
#ifndef BOOST_NO_RTTI
        //! Tests if a class is polymorphic.
        //!
        //! Evaluates to `true` if `Class` is a polymorphic class, as defined by
        //! the C++ standard, i.e. a class that contains at least one virtual
        //! function.
        //!
        //! @tparam Class A class.
        template<class Class>
        static constexpr bool is_polymorphic = std::is_polymorphic_v<Class>;

        //! Returns the static @ref type_id of a type.
        //!
        //! Returns `&typeid(Class)`, cast to `type_id`.
        //!
        //! @tparam Class A class.
        //! @return The static type_id of Class.
        template<class Class>
        static auto static_type() -> type_id {
            return &typeid(Class);
        }

        //! Returns the dynamic @ref type_id of an object.
        //!
        //! Returns `&typeid(obj)`, cast to `type_id`.
        //!
        //! @tparam Class A registered class.
        //! @param obj A reference to an instance of `Class`.
        //! @return The type_id of `obj`'s class.
        template<class Class>
        static auto dynamic_type(const Class& obj) -> type_id {
            return &typeid(obj);
        }

        //! Writes a representation of a @ref type_id to a stream.
        //!
        //! Writes the demangled name of the class identified by `type` to
        //! `stream`.
        //!
        //! @tparam Stream A SimpleOutputStream.
        //! @param type The `type_id` to write.
        //! @param stream The stream to write to.
        template<typename Stream>
        static auto type_name(type_id type, Stream& stream) -> void {
            stream << boost::core::demangle(
                reinterpret_cast<const std::type_info*>(type)->name());
        }

        //! Returns a key that identifies a class across modules.
        //!
        //! C++ does *not* guarantee that there is a single instance of
        //! `std::type_info` per type: a class used by several modules of a
        //! program typically has one per module. `type_index` maps a `type_id`
        //! to a key that compares equal for all the type ids of one class,
        //! which is what @ref initialize uses to group the registrations coming
        //! from different modules. The key must also tell apart two classes
        //! that have the same name, which classes with internal linkage in
        //! different translation units can.
        //!
        //! With MSVC, and with GCC and libstdc++, the key is a
        //! `std::type_index`. Their `std::type_info` equality compares names,
        //! except for classes with internal linkage, which MSVC names
        //! uniquely, and which GCC marks for libstdc++ to compare by address.
        //!
        //! Elsewhere, `std::type_index` does not do. libc++ compares the
        //! `std::type_info` objects of a class with hidden visibility by
        //! address, so the copies in different modules are unequal, and Clang
        //! does not mark the classes with internal linkage for libstdc++. The
        //! key is then the mangled name of the class, plus its `type_id` if
        //! the mangled name shows that the class has internal linkage, or
        //! none.
        //!
        //! @param type A `type_id`.
        //! @return A key identifying the class of `type`.
        static auto type_index(type_id type) {
            const auto& info = *reinterpret_cast<const std::type_info*>(type);

#if BOOST_OPENMETHOD_DETAIL_STD_TYPE_INDEX
            return std::type_index(info);
#else
            const char* name = info.name();

            return detail::type_key{
                name, detail::itanium_internal_linkage(name) ? type : nullptr
            };
#endif
        }

        //! Casts an object to a type.
        //!
        //! Casts `obj` to a reference to an instance of `D`, using
        //! `dynamic_cast`.
        //!
        //! @tparam D A reference to a subclass of `B`.
        //! @tparam B A registered class.
        //! @param obj A reference to an instance of `B`.
        template<typename D, typename B>
        static auto dynamic_cast_ref(B&& obj) -> D {
            return dynamic_cast<D>(obj);
        }
#endif
    };
};

} // namespace boost::openmethod::policies

#endif
