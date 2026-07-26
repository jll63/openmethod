// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_MACROS_HPP
#define BOOST_OPENMETHOD_MACROS_HPP

#include <boost/preprocessor/cat.hpp>

#include <boost/openmethod/core.hpp>

namespace boost::openmethod::detail {

template<typename, class Method, typename ReturnType, typename... Parameters>
struct enable_forwarder;

template<class Method, typename ReturnType, typename... Parameters>
struct enable_forwarder<
    std::void_t<decltype(Method::fn(std::declval<Parameters>()...))>, Method,
    ReturnType, Parameters...> {
    using type = ReturnType;
};

template<class...>
struct va_args;

template<class ReturnType>
struct va_args<ReturnType> {
    using return_type = ReturnType;
    using registry = macro_default_registry;
};

template<class ReturnType, class Registry>
struct va_args<ReturnType, Registry> {
    using return_type = ReturnType;
    using registry = Registry;
};

template<typename...>
inline constexpr bool method_not_found = false;

} // namespace boost::openmethod::detail

#define BOOST_OPENMETHOD_GENSYM BOOST_PP_CAT(openmethod_gensym_, __COUNTER__)

#define BOOST_OPENMETHOD_REGISTER(...)                                         \
    static __VA_ARGS__ BOOST_OPENMETHOD_GENSYM

#define BOOST_OPENMETHOD_ID(NAME) NAME##_boost_openmethod

#define BOOST_OPENMETHOD_OVERRIDERS(NAME)                                      \
    BOOST_PP_CAT(BOOST_OPENMETHOD_ID(NAME), _overriders)

#define BOOST_OPENMETHOD_OVERRIDER(NAME, ARGS, ...)                            \
    BOOST_OPENMETHOD_OVERRIDERS(NAME)<__VA_ARGS__ ARGS>

#define BOOST_OPENMETHOD_GUIDE(NAME)                                           \
    BOOST_PP_CAT(BOOST_OPENMETHOD_ID(NAME), _guide)

#define BOOST_OPENMETHOD_TYPE(NAME, ARGS, ...)                                 \
    ::boost::openmethod::method<                                               \
        BOOST_OPENMETHOD_ID(NAME),                                             \
        ::boost::openmethod::detail::va_args<__VA_ARGS__>::return_type ARGS,   \
        ::boost::openmethod::detail::va_args<__VA_ARGS__>::registry>

#define BOOST_OPENMETHOD(NAME, ARGS, ...)                                      \
    struct BOOST_OPENMETHOD_ID(NAME);                                          \
    template<typename... ForwarderParameters>                                  \
    typename ::boost::openmethod::detail::enable_forwarder<                    \
        void, BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__),                  \
        typename BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__),               \
        ForwarderParameters...>::type                                          \
        BOOST_OPENMETHOD_GUIDE(NAME)(ForwarderParameters && ... args);         \
    template<typename... ForwarderParameters>                                  \
    inline auto NAME(ForwarderParameters&&... args) ->                         \
        typename ::boost::openmethod::detail::enable_forwarder<                \
            void, BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__),              \
            ::boost::openmethod::detail::va_args<__VA_ARGS__>::return_type,    \
            ForwarderParameters...>::type {                                    \
        return BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__)::fn(             \
            std::forward<ForwarderParameters>(args)...);                       \
    }                                                                          \
    template<typename...>                                                      \
    struct BOOST_OPENMETHOD_OVERRIDERS(NAME)

#define BOOST_OPENMETHOD_DETAIL_LOCATE_METHOD(NAME, ARGS)                      \
    template<typename T, typename = void>                                      \
    struct boost_openmethod_detail_locate_method_aux {                         \
        static_assert(                                                         \
            ::boost::openmethod::detail::method_not_found<T>,                  \
            "BOOST_OPENMETHOD_OVERRIDE: cannot find '" #NAME                   \
            "' method that accepts the same arguments as the overrider");      \
    };                                                                         \
    template<typename... A>                                                    \
    struct boost_openmethod_detail_locate_method_aux<                          \
        void(A...),                                                            \
        std::void_t<decltype(BOOST_OPENMETHOD_GUIDE(NAME)(                     \
            std::declval<A>()...))>> {                                         \
        using type =                                                           \
            decltype(BOOST_OPENMETHOD_GUIDE(NAME)(std::declval<A>()...));      \
    }

#define BOOST_OPENMETHOD_DECLARE_OVERRIDER(NAME, ARGS, ...)                    \
    template<typename...>                                                      \
    struct BOOST_OPENMETHOD_OVERRIDERS(NAME);                                  \
    template<>                                                                 \
    struct BOOST_OPENMETHOD_OVERRIDERS(NAME)<__VA_ARGS__ ARGS> {               \
        BOOST_OPENMETHOD_DETAIL_LOCATE_METHOD(NAME, ARGS);                     \
        static auto fn ARGS->__VA_ARGS__;                                      \
        static auto has_next() -> bool;                                        \
        template<typename... Args>                                             \
        static auto next(Args&&... args) -> decltype(auto);                    \
    };                                                                         \
    inline auto BOOST_OPENMETHOD_OVERRIDERS(                                   \
        NAME)<__VA_ARGS__ ARGS>::has_next() -> bool {                          \
        return boost_openmethod_detail_locate_method_aux<                      \
            void ARGS>::type::has_next<fn>();                                  \
    }                                                                          \
    template<typename... Args>                                                 \
    inline auto BOOST_OPENMETHOD_OVERRIDERS(NAME)<__VA_ARGS__ ARGS>::next(     \
        Args&&... args) -> decltype(auto) {                                    \
        return boost_openmethod_detail_locate_method_aux<                      \
            void ARGS>::type::next<fn>(std::forward<Args>(args)...);           \
    }

// REGISTRAR selects which of method<...>::override<Fn> (plain) or
// method<...>::inline_override<Fn> (see core.hpp) registers the overrider.
// Unlike a runtime flag, this is a compile-time choice baked into the
// registrar's own type, so it can't be affected by static-initialization
// ordering (see the comment on class override/inline_override in core.hpp
// for why a runtime constructor argument doesn't work here). Everything
// else is unchanged from - and exactly as comma-safe as -
// BOOST_OPENMETHOD_REGISTER itself: REGISTRAR is a bare identifier (no
// commas to worry about), and the fully-variadic BOOST_OPENMETHOD_REGISTER
// still captures the whole trailing type expression (built from the
// overrider's return type, which may contain an unprotected top-level comma,
// e.g. an un-aliased std::pair<A, B>) as one argument.
#define BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER_AUX(                        \
    NAME, ARGS, REGISTRAR, ...)                                                \
    BOOST_OPENMETHOD_REGISTER(                                                 \
        BOOST_OPENMETHOD_OVERRIDERS(NAME) < __VA_ARGS__ ARGS >                 \
        ::boost_openmethod_detail_locate_method_aux<void ARGS>::type::         \
            REGISTRAR<                                                         \
                BOOST_OPENMETHOD_OVERRIDERS(NAME) < __VA_ARGS__ ARGS>::fn >);

#define BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER(NAME, ARGS, ...)            \
    BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER_AUX(                            \
        NAME, ARGS, override, __VA_ARGS__)

#define BOOST_OPENMETHOD_DEFINE_OVERRIDER(NAME, ARGS, ...)                     \
    BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER(NAME, ARGS, __VA_ARGS__)        \
    auto BOOST_OPENMETHOD_OVERRIDER(NAME, ARGS, __VA_ARGS__)::fn ARGS          \
        -> boost::mp11::mp_back<boost::mp11::mp_list<__VA_ARGS__>>

#define BOOST_OPENMETHOD_OVERRIDE(NAME, ARGS, ...)                             \
    BOOST_OPENMETHOD_DECLARE_OVERRIDER(NAME, ARGS, __VA_ARGS__)                \
    BOOST_OPENMETHOD_DEFINE_OVERRIDER(NAME, ARGS, __VA_ARGS__)

// Unlike BOOST_OPENMETHOD_OVERRIDE, registers via method<...>::inline_override
// instead of method<...>::override, marking the overrider_info as
// inline_ = true (see core.hpp and overrider_info in preamble.hpp), which
// makes it eligible for cross-module dedup during augment_methods()
// consolidation. Only an overrider defined `inline` can legally have an
// identical definition appear in more than one translation unit/module in
// the first place, which is why plain BOOST_OPENMETHOD_OVERRIDE never sets
// this.
#define BOOST_OPENMETHOD_INLINE_OVERRIDE(NAME, ARGS, ...)                      \
    BOOST_OPENMETHOD_DECLARE_OVERRIDER(NAME, ARGS, __VA_ARGS__)                \
    BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER_AUX(                            \
        NAME, ARGS, inline_override, __VA_ARGS__)                              \
    inline auto BOOST_OPENMETHOD_OVERRIDER(NAME, ARGS, __VA_ARGS__)::fn ARGS   \
        -> boost::mp11::mp_back<boost::mp11::mp_list<__VA_ARGS__>>

#define BOOST_OPENMETHOD_CLASSES(...)                                          \
    BOOST_OPENMETHOD_REGISTER(::boost::openmethod::use_classes<__VA_ARGS__>)

// Share a custom registry's state across shared-library boundaries. All of a
// registry's mutable state lives in a single variable,
// registry_state<REGISTRY::registry_type>::st (see registry_state in
// preamble.hpp); these macros emit the explicit instantiation that turns it
// into a single shared symbol. Use them at namespace scope, after the
// registry's definition, with a trailing `;`.
//
// BOOST_OPENMETHOD_EXPORT_REGISTRY expands to an explicit instantiation
// *definition*, so it can also be prefixed with `extern` to make it a
// *declaration*. That gives the three placements needed:
//
//     // my_registry.hpp
//     struct my_registry : boost::openmethod::registry</* ... */> {};
//     #ifdef MY_MODULE_OWNS_REGISTRY
//     extern BOOST_OPENMETHOD_EXPORT_REGISTRY(my_registry);
//     #else
//     BOOST_OPENMETHOD_IMPORT_REGISTRY(my_registry);
//     #endif
//
//     // registry.cpp - exactly one TU of the owning module
//     #define MY_MODULE_OWNS_REGISTRY
//     #include "my_registry.hpp"
//     BOOST_OPENMETHOD_EXPORT_REGISTRY(my_registry);
//
// The plain (definition) form is an explicit instantiation definition, of which
// a program may contain only one, so it belongs in a .cpp - never in a header,
// where every translation unit of the owning module including it would emit one
// (a hard error within one TU; ill-formed, no diagnostic required across TUs).
//
// The `extern` form is what makes a multi-translation-unit owning module work,
// and is required as soon as the owner has more than one. Like the import side
// it suppresses implicit instantiation, and it pins the symbol to default
// visibility / dllexport. Without it, a translation unit of the owner that does
// not emit the export instantiates the state implicitly; under
// -fvisibility=hidden that copy is module-local, ELF merges COMDATs at the
// *most restrictive* visibility, and the result is a local symbol that clients
// cannot link against. A single-translation-unit owner can use the plain form
// alone.
//
// BOOST_OPENMETHOD_IMPORT_REGISTRY already includes the `extern`: an import is
// always a declaration.
//
// The predefined registries are controlled by define-before-include tokens
// instead, and so have a third one for this case:
// BOOST_OPENMETHOD_{EXPORT,DECLARE_EXPORT,IMPORT}_{DEFAULT,INDIRECT}_REGISTRY.
// See shared_libraries.adoc for the full discussion, including the required
// link setup.
#define BOOST_OPENMETHOD_EXPORT_REGISTRY(REGISTRY)                             \
    template struct BOOST_SYMBOL_EXPORT ::boost::openmethod::registry_state<   \
        REGISTRY::registry_type>

#define BOOST_OPENMETHOD_IMPORT_REGISTRY(REGISTRY)                             \
    extern template struct BOOST_SYMBOL_IMPORT ::boost::openmethod::           \
        registry_state<REGISTRY::registry_type>

#endif
