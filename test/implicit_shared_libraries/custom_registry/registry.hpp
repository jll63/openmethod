// Copyright (c) 2018-2027 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_IMPLICIT_SHARED_LIBRARIES_CUSTOM_REGISTRY_HPP
#define BOOST_OPENMETHOD_TEST_IMPLICIT_SHARED_LIBRARIES_CUSTOM_REGISTRY_HPP

#include <boost/mp11/algorithm.hpp>
#include <boost/openmethod/default_registry.hpp>
#include <boost/openmethod/policies/vptr_map.hpp>

// default_registry, with the "vector" and "hash" policies removed and vptr_map
// in their place. `with<vptr_map<>>` *replaces* vptr_vector: both derive from
// the policies::vptr category, and `with` replaces by category (see with_aux in
// preamble.hpp). `without<type_hash>` then drops fast_perfect_hash, which
// vptr_map does not use - unlike vptr_vector, it keys its map on the type_id
// directly. The resulting policy list is:
//
//     std_rtti, vptr_map<>, default_error_handler, stderr_output
struct custom_registry : boost::openmethod::default_registry::with<
                             boost::openmethod::policies::vptr_map<>>::
                             without<boost::openmethod::policies::type_hash> {};

// Both removals above happen implicitly, by category, so assert them.
static_assert(boost::mp11::mp_contains<
              custom_registry::policy_list,
              boost::openmethod::policies::vptr_map<>>::value);
static_assert(!boost::mp11::mp_contains<
              custom_registry::policy_list,
              boost::openmethod::policies::vptr_vector>::value);
static_assert(!boost::mp11::mp_contains<
              custom_registry::policy_list,
              boost::openmethod::policies::fast_perfect_hash>::value);

// Must be set before core.hpp is included (just below), so that the
// BOOST_OPENMETHOD macros, virtual_ptr and unique_virtual_ptr all default to
// custom_registry.
#define BOOST_OPENMETHOD_DEFAULT_REGISTRY custom_registry

#include <boost/openmethod.hpp>

// Where each macro goes:
//
//   extern template ... BOOST_SYMBOL_IMPORT - header, every client TU
//   extern template ... BOOST_SYMBOL_EXPORT - header, every owning-module TU
//   template ...        (no attribute)      - exactly one .cpp of the owner
//
// The two declarations may be repeated and so live here. The definition, of
// which a program may contain only one, lives in a .cpp (lib.cpp) - never in a
// header, where every TU of the owning module including it would emit one. It
// carries no visibility attribute: this header already declared the symbol
// exported, and GCC rejects repeating the attribute on the definition.
//
// The exported *declaration* is what makes a multi-TU owning module work.
// Without it, a TU of the owner that does not emit the definition instantiates
// the state implicitly; under -fvisibility=hidden that copy is module-local,
// ELF merges COMDATs at the most restrictive visibility, and the symbol ends up
// local so clients cannot link. lib2.cpp is a second TU of this library
// precisely to keep that path covered.
#if defined(OWNS_REGISTRY_STATE)
BOOST_OPENMETHOD_EXPORT_REGISTRY(custom_registry);
#else
BOOST_OPENMETHOD_IMPORT_REGISTRY(custom_registry);
#endif

#endif
