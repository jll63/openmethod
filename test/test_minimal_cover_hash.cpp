// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE minimal_perfect_hash
#include <boost/test/unit_test.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/minimal_cover_hash.hpp>
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
          test_rtti, minimal_cover_hash, stderr_output, throw_error_handler> {};

type_id test_ids[] = {
    type_id(0x0000000000d90810), type_id(0x00000000014f3cf0),
    type_id(0x00000000015040c0), type_id(0x00000000014eecb8),
    type_id(0x00000000014f37d8), type_id(0x00000000014f0558),
    type_id(0x00000000014f1220), type_id(0x00000000014f2040),
    type_id(0x00000000014f0908), type_id(0x00000000014f2a60),
    type_id(0x0000000001502d98), type_id(0x00000000014eef10),
    type_id(0x00000000015010f8), type_id(0x00000000014ef5e0),
    type_id(0x00000000015049e0), type_id(0x00000000014f2298),
    type_id(0x00000000014f0b90), type_id(0x00000000015030b8),
    type_id(0x00000000015045b0), type_id(0x00000000014f3958),
    type_id(0x0000000000d90488), type_id(0x0000000001504818),
    type_id(0x0000000001504518), type_id(0x00000000014f3918),
    type_id(0x00000000014f0f58), type_id(0x0000000000d907c8),
    type_id(0x0000000000d90650), type_id(0x00000000014f2a48),
    type_id(0x00000000014f1fc8), type_id(0x00000000014ef170),
    type_id(0x00000000014eee88), type_id(0x0000000000d906e8),
    type_id(0x0000000001503f68), type_id(0x0000000001503f90),
    type_id(0x0000000001501e08), type_id(0x00000000014f49b0),
    type_id(0x0000000000db0f08), type_id(0x00000000014f0c08),
    type_id(0x00000000014f42b8), type_id(0x00000000014f20a8),
    type_id(0x00000000014f10a0), type_id(0x0000000000db0f50),
    type_id(0x00000000014f1428), type_id(0x00000000014f2d50),
    type_id(0x00000000014f00a0), type_id(0x00000000014f01e0),
    type_id(0x00000000014f22b0), type_id(0x00000000014f0b10),
    type_id(0x0000000001503ed0), type_id(0x0000000001500e88),
    type_id(0x00000000014f1130), type_id(0x0000000001503858),
    type_id(0x00000000014f2158), type_id(0x00000000014f1180),
    type_id(0x0000000000d90368), type_id(0x00000000014f35d8),
    type_id(0x00000000014f06f0), type_id(0x00000000014ef360),
    type_id(0x00000000014f3e88), type_id(0x0000000001502f48),
    type_id(0x00000000014f3888), type_id(0x00000000015046e0),
    type_id(0x0000000000d90868), type_id(0x00000000014f4a80),
    type_id(0x00000000014ee6b8), type_id(0x00000000014f2dd8),
    type_id(0x00000000014ef068), type_id(0x00000000014ef2b8),
    type_id(0x00000000014fefe0), type_id(0x0000000001504648),
    type_id(0x0000000000d90328), type_id(0x0000000001504288),
    type_id(0x00000000015041f0), type_id(0x00000000014f4ac0),
    type_id(0x00000000014f4968), type_id(0x0000000001504150),
    type_id(0x0000000001503310), type_id(0x00000000014f4480),
    type_id(0x0000000001503058), type_id(0x00000000014f3438),
    type_id(0x00000000014f20d0), type_id(0x00000000014f1ae0),
    type_id(0x00000000014f1378), type_id(0x0000000000d90778),
    type_id(0x00000000014f4690), type_id(0x00000000014eef20),
    type_id(0x00000000014ff130), type_id(0x00000000014f46f8),
    type_id(0x00000000014f38a0), type_id(0x00000000014f04f8),
    type_id(0x00000000014f31f8), type_id(0x00000000014f0210),
    type_id(0x00000000014fee98), type_id(0x00000000014f07f8),
    type_id(0x00000000014f0c88), type_id(0x0000000001503700),
    type_id(0x00000000014f06a0), type_id(0x00000000014f09e0),
    type_id(0x00000000015049c8), type_id(0x00000000014f1b08),
    type_id(0x0000000001503c48), type_id(0x00000000014fec90),
    type_id(0x00000000014ff0f8), type_id(0x00000000014f0f88),
    type_id(0x00000000014f4248), type_id(0x00000000014f2c18),
    type_id(0x00000000014f4770), type_id(0x00000000014f3b20),
    type_id(0x00000000014f1170), type_id(0x00000000014ef128),
    type_id(0x00000000014f42e0), type_id(0x00000000014f48c8),
    type_id(0x00000000014f0008), type_id(0x0000000001504950),
    type_id(0x00000000014f49f0), type_id(0x00000000014f3410),
    type_id(0x00000000014f20b8), type_id(0x00000000014f31d0),
    type_id(0x00000000014ff760), type_id(0x00000000014ff070),
    type_id(0x0000000000d90590), type_id(0x00000000014f42f8),
    type_id(0x00000000014f2280), type_id(0x00000000014eeea0),
    type_id(0x00000000014ef388), type_id(0x00000000014f1070),
    type_id(0x00000000014f3b08), type_id(0x0000000000d90530),
    type_id(0x00000000015032a0), type_id(0x00000000015042d0),
    type_id(0x00000000014f0740), type_id(0x0000000000d90880),
    type_id(0x00000000014fefa0), type_id(0x0000000001503e80),
    type_id(0x00000000014ef078), type_id(0x0000000001503d20),
    type_id(0x00000000014feef8), type_id(0x00000000014ff6d8),
    type_id(0x0000000001503fa8), type_id(0x0000000001504458),
    type_id(0x0000000001504240), type_id(0x00000000014ff1f8),
    type_id(0x00000000015048b8), type_id(0x00000000014ef088),
    type_id(0x0000000001503750), type_id(0x0000000001503010),
    type_id(0x0000000000d905f0), type_id(0x0000000001504778),
    type_id(0x00000000014f3ef0), type_id(0x00000000014f3740),
    type_id(0x0000000001503780), type_id(0x00000000014f0868),
    type_id(0x00000000014f0d08), type_id(0x00000000014f08d0),
    type_id(0x00000000014f0468), type_id(0x00000000014f0f70),
    type_id(0x0000000001502e70), type_id(0x00000000014f1688),
    type_id(0x0000000001504320), type_id(0x0000000001503768),
    type_id(0x00000000015037e0), type_id(0x00000000014ff188),
    type_id(0x00000000014f08a0), type_id(0x0000000001504860),
    type_id(0x0000000001504198),
};

BOOST_AUTO_TEST_CASE(test_minimal_cover_hash) {
    //BOOST_TEST_MESSAGE("Test iteration " << (i + 1));

    test_registry::compiler ctx{trace()};
    constexpr auto ids_count = sizeof(test_ids) / sizeof(test_ids[0]);
    ctx.classes.resize(ids_count);

    std::transform(test_ids, test_ids + ids_count, ctx.classes.begin(), [&](type_id id) {
        detail::generic_compiler::class_ cls;
        cls.type_ids.push_back(id);
        return cls;
    });

    BOOST_REQUIRE_NO_THROW(
        test_registry::policy<type_hash>::initialize(ctx, std::tuple<trace>()));
}
