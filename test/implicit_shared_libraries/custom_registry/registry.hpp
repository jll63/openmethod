// Copyright (c) 2018-2025 Jean-Louis Leroy
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

// A custom registry has no define-before-include macro pair of its own, so use
// the general, registry-parameterized pair, at namespace scope, after the
// registry's definition. lib.cpp defines LIB_SOURCE: it owns and exports the
// state, and every other translation unit imports it.
#if defined(LIB_SOURCE)
BOOST_OPENMETHOD_EXPORT_REGISTRY(custom_registry);
#else
BOOST_OPENMETHOD_IMPORT_REGISTRY(custom_registry);
#endif

#endif
