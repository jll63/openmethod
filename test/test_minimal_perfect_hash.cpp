// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE minimal_perfect_hash
#include <boost/test/unit_test.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/minimal_perfect_hash.hpp>
#include <boost/openmethod/policies/std_rtti.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>
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

struct test_rtti : rtti {
    template<class Registry>
    struct fn : defaults {
        template<typename Stream>
        static auto type_name(type_id type, Stream& stream) {
            stream << type;
        }
    };
};

struct test_registry
    : registry<
          test_rtti, minimal_perfect_hash, stderr_output, throw_error_handler> {
};

BOOST_AUTO_TEST_CASE(scattered_ids) {
    std::default_random_engine rnd;
    std::uniform_int_distribution<std::uintptr_t> dist;

    for (int i = 0; i < 100; ++i) {
        BOOST_TEST_MESSAGE("Test iteration " << (i + 1));

        // Create a set of unique IDs
        std::unordered_set<type_id> unique_ids;

        while (unique_ids.size() < 20'000) {
            unique_ids.insert(reinterpret_cast<type_id>(dist(rnd)));
        }

        BOOST_TEST_MESSAGE("ids:");
        std::size_t count = 0;

        for (auto id : unique_ids) {
            BOOST_TEST_MESSAGE("  " << id);

            if (++count == 5) {
                BOOST_TEST_MESSAGE("  ...");
                break;
            }
        }

        test_registry::compiler ctx;
        ctx.classes.resize(unique_ids.size());
        std::transform(
            unique_ids.begin(), unique_ids.end(), ctx.classes.begin(),
            [&](type_id id) {
                detail::generic_compiler::class_ cls;
                cls.type_ids.push_back(id);
                return cls;
            });

        BOOST_REQUIRE_NO_THROW(test_registry::policy<type_hash>::initialize(
            ctx, std::tuple<trace>()));

        std::unordered_map<std::size_t, type_id> indexes;

        for (auto id : unique_ids) {
            auto index = test_registry::policy<type_hash>::hash(id);
            bool found = indexes.find(index) != indexes.end();
            BOOST_CHECK(!found);

            if (found) {
                BOOST_TEST_MESSAGE(
                    "Collision at index " << index << ": " << id << " <> "
                                          << indexes[index]);
            }

            indexes[index] = id;
        }
    }
}

auto foo() {
    return test_registry::policy<type_hash>::hash(type_id(123456));
}
