// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// std_rtti::type_index must be equal for the copies of a class in different
// modules, and different for two classes with internal linkage that have the
// same name. Where std::type_index does not do that, it scans the mangled name
// for internal linkage.

#include <boost/openmethod.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include <typeinfo>

using boost::openmethod::default_registry;
using boost::openmethod::detail::itanium_internal_linkage;

// Names produced by GCC, Clang, and Clang with libc++.
BOOST_AUTO_TEST_CASE(mangled_names) {
    const char* shared[] = {
        "6Global",
        "N2ns5InnerE",
        "10ZLibStream",   // not `ZL`, the mark of a static entity
        "12UtilityClass", // not `Ut`, the mark of an unnamed type
        "3BoxISt3mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt6"
        "vectorIiSaIiEESt4lessIS6_ESaISt4pairIKS6_S9_EEEE",
        "3BoxINSt3__13mapINS0_12basic_stringIcNS0_11char_traitsIcEENS0_"
        "9allocatorIcEEEENS0_6vectorIiNS5_IiEEEENS0_4lessIS7_EENS5_INS0_"
        "4pairIKS7_SA_EEEEEEE",
        "4VBoxIL5Color12EE",           // an enum literal, not a static entity
        "4VBoxILN2ns5ShadeE12EE",      // the same, of a nested enum
        "4VBoxILin3EE",                // a negative integer literal
        "4VBoxIXadL_Z10extern_varEEE", // the address of an extern variable
        "3BoxIA10_iE",
        "Z15local_in_externvE5Local", // local to a function defined once
        "N5boost10openmethod6methodI22speak_boost_openmethodFvNS0_11virtual_"
        "ptrI6GlobalNS0_16default_registryEvEESt3mapINSt7__cxx1112basic_"
        "stringIcSt11char_traitsIcESaIcEEESt6vectorIiSaIiEESt4lessISD_ESaISt4"
        "pairIKSD_SG_EEEES5_EE",
    };

    for (auto name : shared) {
        BOOST_TEST_INFO(name);
        BOOST_TEST(!itanium_internal_linkage(name));
    }

    const char* internal[] = {
        "N12_GLOBAL__N_14AnonE",
        "3BoxIN12_GLOBAL__N_14AnonEE",
        "4VBoxIXadL_ZL10static_varEEE",
        "ZL15local_in_staticvE5Local",
        "ZN2nsL15local_in_staticEvE5Local",
        "NL6lambdaMUlvE_E", // GCC: a lambda in a static variable
        "3$_0",             // Clang: an unnamed type, or a lambda
        "10._anon_100",     // GCC: an unnamed type
    };

    for (auto name : internal) {
        BOOST_TEST_INFO(name);
        BOOST_TEST(itanium_internal_linkage(name));
    }
}

struct Global {};

struct Other {};

namespace {
struct Anon {};
} // namespace

template<class T>
auto key() {
    return default_registry::rtti::type_index(
        default_registry::rtti::static_type<T>());
}

BOOST_AUTO_TEST_CASE(keys) {
    BOOST_TEST((key<Global>() == key<Global>()));
    BOOST_TEST((key<Global>() != key<Other>()));
    BOOST_TEST((key<Global>() < key<Other>() || key<Other>() < key<Global>()));

#if !BOOST_OPENMETHOD_DETAIL_STD_TYPE_INDEX
    // The scanner is in use: check it on the names of this compiler.
    BOOST_TEST(key<Global>().local == nullptr);
    BOOST_TEST(
        key<Anon>().local == default_registry::rtti::static_type<Anon>());
    BOOST_TEST(!itanium_internal_linkage(typeid(Global).name()));
    BOOST_TEST(itanium_internal_linkage(typeid(Anon).name()));
#endif
}
