// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_DETAIL_REFLECTION_HPP
#define BOOST_OPENMETHOD_DETAIL_REFLECTION_HPP

// Detect C++26 reflection (P2996). Both the language feature (the `^^`
// operator) and the library (`std::meta`) are required.
#if !defined(BOOST_OPENMETHOD_HAS_REFLECTION)
#if defined(__cpp_impl_reflection) && __has_include(<meta>)
#include <meta>
#if defined(__cpp_lib_reflection)
#define BOOST_OPENMETHOD_HAS_REFLECTION 1
#endif
#endif
#endif

#if !defined(BOOST_OPENMETHOD_HAS_REFLECTION)
#define BOOST_OPENMETHOD_HAS_REFLECTION 0
#endif

#if BOOST_OPENMETHOD_HAS_REFLECTION

#include <vector>

#include <boost/mp11/list.hpp>

namespace boost::openmethod::detail {

// =============================================================================
// base classes

// Append `type` and all the base classes transitively reachable from it to
// `types`, skipping the ones already present. Only public base specifiers are
// followed: a class reached solely through a private or protected base cannot
// take part in dispatch, because the conversion is not available to the
// library.
consteval void collect_reflected_bases(
    std::meta::info type, std::vector<std::meta::info>& types) {
    for (auto seen : types) {
        if (seen == type) {
            return;
        }
    }

    types.push_back(type);

    // The range is held in a named local instead of being left to the
    // range-for to lifetime-extend. GCC 16, since r16-8246 (the fix for
    // PR124575), marks a lifetime-extended temporary of consteval-only type -
    // which `vector<meta::info>` is - `DECL_EXTERNAL`. The constant evaluator
    // then hands every frame of a recursive call the same object: the inner
    // call destroys the vector the outer call is still walking, and the loop
    // fails with "accessing '<anonymous>' outside its lifetime". A plain
    // automatic variable is not an extended-ref temporary, so each frame gets
    // its own. `scan_namespace` below recurses too, and does the same.
    auto bases =
        std::meta::bases_of(type, std::meta::access_context::unchecked());

    for (auto base : bases) {
        if (std::meta::is_public(base)) {
            collect_reflected_bases(std::meta::type_of(base), types);
        }
    }
}

template<class Class>
consteval auto reflected_bases_info() -> std::meta::info {
    std::vector<std::meta::info> types;
    collect_reflected_bases(std::meta::dealias(^^Class), types);

    return std::meta::substitute(^^mp11::mp_list, types);
}

// `mp11::mp_list<Class, Bases...>`, where `Bases` are all the base classes
// transitively reachable from `Class` through public inheritance, in
// unspecified order. `Class` itself is the first element.
template<class Class>
// clang-format off: the formatter predates P2996 and eats the spaces around
// the splice, leaving `typename[:...:]`.
using reflected_bases = typename [: reflected_bases_info<Class>() :];
// clang-format on

// =============================================================================
// namespace scan

// True if `member` is the `std` namespace, which a scan does not enter.
// Walking it would cost a great deal and find nothing: a method cannot be
// declared on a class the program has never heard of. The nested namespaces -
// `std::chrono`, `std::ranges`, the inline versioning ones - are reached only
// through their parent, so they are left out with it.
consteval auto is_std_namespace(std::meta::info member) -> bool {
    return std::meta::dealias(member) == ^^::std;
}

consteval auto contains(
    const std::vector<std::meta::info>& types, std::meta::info type) -> bool {
    for (auto seen : types) {
        if (seen == type) {
            return true;
        }
    }

    return false;
}

consteval void push_unique(
    std::vector<std::meta::info>& types, std::meta::info type) {
    if (!contains(types, type)) {
        types.push_back(type);
    }
}

// The class template specialization that `member` names: `member` itself, if it
// is a type - or an alias for one - that is a specialization; or the class that
// encloses `member`'s type, if `member` is a variable of a nested type. This is
// how a `method` is found: the core interface names it in an alias, and a
// registrar - the one `BOOST_OPENMETHOD_OVERRIDE` creates, or one written by
// hand - is a variable of type `method<...>::override<...>`. Returns an invalid
// reflection if `member` names no specialization.
consteval auto specialization_named_by(std::meta::info member)
    -> std::meta::info {
    if (std::meta::is_type(member)) {
        auto type = std::meta::dealias(member);

        if (std::meta::has_template_arguments(type)) {
            return type;
        }

        return std::meta::info();
    }

    if (std::meta::is_variable(member)) {
        auto enclosing = std::meta::type_of(member);

        if (std::meta::has_parent(enclosing)) {
            auto parent = std::meta::parent_of(enclosing);

            if (std::meta::is_type(parent) &&
                std::meta::has_template_arguments(parent)) {
                return parent;
            }
        }
    }

    return std::meta::info();
}

// Walk `ns` and the namespaces nested in it, collecting the specializations of
// `Template` that its members name, and the complete class types they declare.
// Nothing else is retained: the scan of a large namespace must not build a list
// of everything in it.
consteval void scan_namespace(
    std::meta::info ns, std::meta::info Template,
    std::vector<std::meta::info>& specializations,
    std::vector<std::meta::info>& classes) {
    // A named local, for the reason given in `collect_reflected_bases`.
    auto members =
        std::meta::members_of(ns, std::meta::access_context::unchecked());

    for (auto member : members) {
        if (std::meta::is_namespace(member)) {
            if (!is_std_namespace(member)) {
                scan_namespace(member, Template, specializations, classes);
            }

            continue;
        }

        auto specialization = specialization_named_by(member);

        if (specialization != std::meta::info() &&
            std::meta::template_of(specialization) == Template) {
            push_unique(specializations, specialization);
        }

        if (std::meta::is_type(member)) {
            auto type = std::meta::dealias(member);

            if (std::meta::is_class_type(type) &&
                std::meta::is_complete_type(type)) {
                push_unique(classes, type);
            }
        }
    }
}

} // namespace boost::openmethod::detail

#endif

#endif
