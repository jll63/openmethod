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

// Only the *import* side belongs in a header. BOOST_OPENMETHOD_IMPORT_REGISTRY
// expands to an `extern template` declaration, which may be repeated freely.
// BOOST_OPENMETHOD_EXPORT_REGISTRY expands to an explicit instantiation
// *definition*, of which a program may contain only one, so it belongs in a
// .cpp file - lib.cpp, here - never in a header: every translation unit of the
// owning module that included such a header would emit one. In a single
// translation unit that is a hard error ("duplicate explicit instantiation");
// across translation units it is ill-formed, no diagnostic required - ELF
// toolchains happen to merge the COMDAT silently, which is not something to
// rely on.
//
// Every translation unit of the module that owns the state defines
// OWNS_REGISTRY_STATE before including this header, so that none of them
// imports it; exactly one of them (lib.cpp) emits the export. Client modules
// define nothing and import.
//
// NOTE: this library has a single translation unit. A multi-TU owning module
// cannot currently be expressed with these macros under hidden visibility: the
// TUs that do not emit the export implicitly instantiate the state as a hidden
// COMDAT, and ELF merges COMDATs with the most restrictive visibility, so the
// symbol ends up local and clients fail to link.
#if !defined(OWNS_REGISTRY_STATE)
BOOST_OPENMETHOD_IMPORT_REGISTRY(custom_registry);
#endif

#endif
