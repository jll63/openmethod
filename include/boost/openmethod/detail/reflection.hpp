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

#include <cstddef>
#include <type_traits>
#include <vector>

namespace boost::openmethod::detail {

// One argument of `register_classes`: a group of reflections, written in
// braces - or, for a group of one, as the reflection itself, which the
// converting constructor turns into a group of one.
//
// The constructor is constrained, so that it cannot be picked over the copy
// constructor, and `consteval`, as `std::meta::info` is a consteval-only type.
//
// `Size` is deduced from the number of items; the array is never empty, as
// `{}` must produce a valid type too.
template<std::size_t Size>
struct reflection_group {
    std::meta::info items[Size ? Size : 1]{};
    std::size_t size = Size;

    consteval reflection_group() = default;

    template<class... T>
        requires(... && std::is_same_v<T, std::meta::info>)
    consteval reflection_group(T... items_) :
        items{items_...}, size(sizeof...(T)) {
    }
};

template<class... T>
reflection_group(T...) -> reflection_group<sizeof...(T)>;
reflection_group() -> reflection_group<0>;

// =============================================================================
// vectors of reflections

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

// =============================================================================
// base classes

// Append `type` and all the base classes transitively reachable from it to
// `types`, skipping the ones already present. Only public base specifiers are
// followed: a class reached solely through a private or protected base cannot
// take part in dispatch, because the conversion is not available to the
// library. A base the walk arrives at again is appended to `repeated` as well:
// it is a candidate for ambiguity, which `collect_dispatchable_bases` decides.
consteval void collect_reflected_bases(
    std::meta::info type, std::vector<std::meta::info>& types,
    std::vector<std::meta::info>& repeated) {
    if (contains(types, type)) {
        push_unique(repeated, type);

        return;
    }

    types.push_back(type);

    // The range is held in a named local instead of being left to the
    // range-for to lifetime-extend, to work around GCC PR124645/PR124646.
    // r16-8235 marks a lifetime-extended temporary of consteval-only type -
    // which `vector<meta::info>` is - `DECL_EXTERNAL` unconditionally, and the
    // constant evaluator then hands every frame of a recursive call the same
    // object: the inner call destroys the vector the outer call is still
    // walking, and the loop fails with "accessing '<anonymous>' outside its
    // lifetime". A plain automatic variable is not an extended-ref temporary,
    // so each frame gets its own. `scan_scope` below recurses too, and does
    // the same.
    //
    // Fixed upstream in r16-8430. Drop this once no supported toolchain sits
    // in between - Ubuntu 26.04, which Boost.CI uses for the C++26 leg, ships
    // 16-20260322 (r16-8246) and does.
    auto bases =
        std::meta::bases_of(type, std::meta::access_context::unchecked());

    for (auto base : bases) {
        if (std::meta::is_public(base)) {
            collect_reflected_bases(std::meta::type_of(base), types, repeated);
        }
    }
}

// True if a `derived` can be converted to a `base` - that is, if `base` is a
// public base class of `derived` and naming it is unambiguous. Exact, as it
// asks the language: a virtual base is one subobject however many paths reach
// it. It costs a template instantiation, so ask it only where the answer can
// differ from what the walk above already knows.
consteval auto is_dispatchable_base(
    std::meta::info derived, std::meta::info base) -> bool {
    return std::meta::is_convertible_type(
        std::meta::add_pointer(derived), std::meta::add_pointer(base));
}

// Append to `types` the classes `type` can dispatch as: itself, and the base
// classes reachable from it through public inheritance, less the ones that
// repeated inheritance makes ambiguous. An ambiguous base cannot take part in
// dispatch - no reference to it can be formed - so it is left out.
// `use_classes` rejects such a hierarchy outright; `register_classes` cannot,
// because it sees classes it was never asked about.
//
// Only the bases the walk arrived at more than once can be ambiguous, and only
// those are put to `is_dispatchable_base`: a hierarchy without repeated
// inheritance instantiates nothing.
consteval void collect_dispatchable_bases(
    std::meta::info type, std::vector<std::meta::info>& types) {
    std::vector<std::meta::info> reachable, repeated;
    collect_reflected_bases(type, reachable, repeated);

    for (auto base : reachable) {
        if (contains(repeated, base) && !is_dispatchable_base(type, base)) {
            continue;
        }

        types.push_back(base);
    }
}

// =============================================================================
// scan

// True if `member` is a namespace that a recursive scan does not enter: `std`
// and `boost`. Walking them would cost a great deal and find nothing: a method
// cannot be declared on a class the program has never heard of. The nested
// namespaces - `std::chrono`, `boost::mp11`, the inline versioning ones - are
// reached only through their parent, so they are left out with it. The
// exclusion applies only to recursion: a namespace listed explicitly is always
// scanned, which is how a class in `std` or `boost` is registered.
consteval auto is_excluded_namespace(std::meta::info member) -> bool {
    auto ns = std::meta::dealias(member);

    return ns == ^^::std || ns == ^^::boost;
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

// Walk `scope` - a namespace, or a class - and the namespaces and classes
// nested in it, collecting the specializations of `Template` that its members
// name, and the complete class types they declare. Nothing else is retained:
// the scan of a large namespace must not build a list of everything in it.
//
// A member that *names* a class registers it, which is how a class reached
// through an alias is found. Recursion is narrower: it enters only a class the
// scope actually *declares*, which `parent_of` answers. Following an alias
// instead would walk whatever it points at - a member `using` for
// `std::string` would drag the whole of `basic_string` in behind it - and
// `std` is excluded from the scan for that very reason.
consteval void scan_scope(
    std::meta::info scope, std::meta::info Template,
    std::vector<std::meta::info>& specializations,
    std::vector<std::meta::info>& classes) {
    // A named local, for the reason given in `collect_reflected_bases`.
    auto members =
        std::meta::members_of(scope, std::meta::access_context::unchecked());

    for (auto member : members) {
        if (std::meta::is_namespace(member)) {
            if (!is_excluded_namespace(member)) {
                scan_scope(member, Template, specializations, classes);
            }

            continue;
        }

        auto specialization = specialization_named_by(member);

        if (specialization != std::meta::info() &&
            std::meta::template_of(specialization) == Template) {
            push_unique(specializations, specialization);
        }

        if (std::meta::is_type(member)) {
            // An alias may add cv-qualification -
            // `using CDog = const Dog` - which is not a distinct class to
            // register: the registry would hold `Dog` and `const Dog` as two
            // lattice nodes, each with its own hash slot and dispatch table
            // row.
            auto type = std::meta::remove_cv(std::meta::dealias(member));

            if (std::meta::is_class_type(type) &&
                std::meta::is_complete_type(type)) {
                auto known = contains(classes, type);
                push_unique(classes, type);

                // The injected class name makes a class a member of itself,
                // and a class already walked may be named again; `known`
                // stops both.
                if (!known && std::meta::has_parent(type) &&
                    std::meta::parent_of(type) == scope) {
                    scan_scope(type, Template, specializations, classes);
                }
            }
        }
    }
}

} // namespace boost::openmethod::detail

#endif

#endif
