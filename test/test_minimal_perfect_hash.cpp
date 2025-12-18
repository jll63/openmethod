// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE minimal_perfect_hash
#include <boost/test/unit_test.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/minimal_perfect_hash.hpp>
#include <boost/openmethod/policies/std_rtti.hpp>
#include <boost/openmethod/policies/vptr_vector.hpp>
#include <boost/openmethod/policies/stderr_output.hpp>
#include <boost/openmethod/policies/default_error_handler.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_util.hpp"

#include <iostream>
#include <string>
#include <random>
#include <vector>

using namespace boost::openmethod;
using namespace boost::openmethod::policies;

struct test_registry : registry<
                           std_rtti, minimal_perfect_hash,
                           stderr_output, runtime_checks> {};

BOOST_AUTO_TEST_CASE(scattered_ids) {

    // Create a set of unique IDs
    std::unordered_set<type_id> unique_ids;
    std::default_random_engine rnd;
    std::uniform_int_distribution<std::uintptr_t> dist;
    while (unique_ids.size() < 20) {
        unique_ids.insert(reinterpret_cast<type_id>(dist(rnd)));
    }

    test_registry::compiler ctx{trace()};
    ctx.classes.resize(unique_ids.size());
    std::transform(
        unique_ids.begin(), unique_ids.end(), ctx.classes.begin(),
        [&](type_id id) {
            detail::generic_compiler::class_ cls;
            cls.type_ids.push_back(id);
            return cls;
        });

    test_registry::policy<type_hash>::initialize(ctx, std::tuple<trace>());

    std::unordered_set<std::size_t> indexes;

    for (auto id : unique_ids) {
        auto index = test_registry::policy<type_hash>::hash(id);
        BOOST_CHECK(index < 1100);
        BOOST_CHECK(indexes.count(index) == 0);
        indexes.insert(index);
    }
}
