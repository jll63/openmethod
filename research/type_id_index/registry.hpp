// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.

#ifndef BOOST_OPENMETHOD_RESEARCH_REGISTRY_HPP
#define BOOST_OPENMETHOD_RESEARCH_REGISTRY_HPP

// This header owns the whole registry recipe, in the manner of
// test/test_checked_registry.hpp: the forward declaration, the
// BOOST_OPENMETHOD_DEFAULT_REGISTRY definition, the library include and the
// registry itself. Force-including it (`-include`, `/FI`) in front of an
// existing test is enough to re-run that test against a prototype policy.
//
// RESEARCH_POLICY selects the configuration under measurement:
//
//   fph      stock default_registry - the control
//   index    fast_perfect_hash replaced by direct_type_id_index<8>
//   index16  ... by direct_type_id_index<16>
//   direct   fast_perfect_hash *and* vptr_vector replaced by
//            vptr_direct_vector
//   pext     fast_perfect_hash replaced by pext_index (needs -mbmi2)
//   hd       fast_perfect_hash replaced by hd_index - a near-minimal
//            perfect hash, ~1.05 slots per type id
//   hdmin    ... by hd_index<2, 100>, an exactly minimal one
//   twolevel ... by two_level_index - h2(x) = (m2[h1(x)] * h) >> s2, a
//            per-bucket multiplier into a power-of-two shared table
//   twolevel1 ... the same with the first-level multiplier taken as it comes,
//            to price the search for one that evens the buckets
//
// The policy lists are spelled out rather than derived from default_registry
// with `with`/`without`: `with` appends, and a type_hash policy must precede
// vptr_vector in the list (vptr_vector static_asserts on it, because policies
// are initialized left to right and it reads the hash's state).

#define BOOST_OPENMETHOD_RESEARCH_FPH 0
#define BOOST_OPENMETHOD_RESEARCH_INDEX 1
#define BOOST_OPENMETHOD_RESEARCH_INDEX16 2
#define BOOST_OPENMETHOD_RESEARCH_DIRECT 3
#define BOOST_OPENMETHOD_RESEARCH_PEXT 4
#define BOOST_OPENMETHOD_RESEARCH_HD 5
#define BOOST_OPENMETHOD_RESEARCH_HDMIN 6
#define BOOST_OPENMETHOD_RESEARCH_TWOLEVEL 7
#define BOOST_OPENMETHOD_RESEARCH_TWOLEVEL1 8

#ifndef RESEARCH_POLICY
#define RESEARCH_POLICY BOOST_OPENMETHOD_RESEARCH_INDEX
#endif

struct research_registry;
#define BOOST_OPENMETHOD_DEFAULT_REGISTRY research_registry

#include <boost/openmethod.hpp>

#include "direct_type_id_index.hpp"
#include "vptr_direct_vector.hpp"

#if RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_PEXT
#include "pext_index.hpp"
#endif

#if RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_HD ||                         \
    RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_HDMIN
#include "hd_index.hpp"
#endif

#if RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_TWOLEVEL ||                   \
    RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_TWOLEVEL1
#include "two_level_index.hpp"
#endif

#ifdef BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS
#define BOOST_OPENMETHOD_RESEARCH_CHECKS                                       \
    , boost::openmethod::policies::runtime_checks
#else
#define BOOST_OPENMETHOD_RESEARCH_CHECKS
#endif

namespace boost::openmethod::research {

#if RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_FPH

#define RESEARCH_POLICY_NAME "fph"
using selected_registry = registry<
    policies::std_rtti, policies::fast_perfect_hash, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_INDEX

#define RESEARCH_POLICY_NAME "index"
using selected_registry = registry<
    policies::std_rtti, direct_type_id_index<8>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_INDEX16

#define RESEARCH_POLICY_NAME "index16"
using selected_registry = registry<
    policies::std_rtti, direct_type_id_index<16>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_DIRECT

// No type_hash at all: vptr_direct_vector folds the base into the table's
// address, so there is nothing left for a separate hash policy to do.
#define RESEARCH_POLICY_NAME "direct"
using selected_registry = registry<
    policies::std_rtti, vptr_direct_vector, policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_PEXT

#define RESEARCH_POLICY_NAME "pext"
using selected_registry = registry<
    policies::std_rtti, pext_index<>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_HD

#define RESEARCH_POLICY_NAME "hd"
using selected_registry = registry<
    policies::std_rtti, hd_index<>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_HDMIN

// Exactly one slot per type id. Halving Lambda is what makes a full table
// affordable, and it doubles the pilot array, which is why this is the more
// expensive of the two in bytes despite wasting no slot.
#define RESEARCH_POLICY_NAME "hdmin"
using selected_registry = registry<
    policies::std_rtti, hd_index<2, 100>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_TWOLEVEL

#define RESEARCH_POLICY_NAME "twolevel"
using selected_registry = registry<
    policies::std_rtti, two_level_index<>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#elif RESEARCH_POLICY == BOOST_OPENMETHOD_RESEARCH_TWOLEVEL1

// One first-level multiplier, whatever buckets it happens to give.
#define RESEARCH_POLICY_NAME "twolevel1"
using selected_registry = registry<
    policies::std_rtti, two_level_index<4, 1>, policies::vptr_vector,
    policies::default_error_handler,
    policies::stderr_output BOOST_OPENMETHOD_RESEARCH_CHECKS>;

#else
#error "RESEARCH_POLICY must be one of fph, index, index16, direct, pext, "    \
    "hd, hdmin, twolevel, twolevel1"
#endif

} // namespace boost::openmethod::research

struct research_registry : boost::openmethod::research::selected_registry {};

#endif
