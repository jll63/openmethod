// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <type_traits>

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/vptr_map.hpp>

using namespace boost::openmethod;

namespace same_policies {

// tag::shared[]
struct animals : registry<policies::std_rtti, policies::vptr_map<>> {};
struct vehicles : registry<policies::std_rtti, policies::vptr_map<>> {};

// the policy lists are identical, so this is one registry, not two
static_assert(std::is_same_v<animals::registry_type, vehicles::registry_type>);
// end::shared[]

} // namespace same_policies

namespace distinct_policies {

// tag::distinct[]
// a policy in a category of its own, carrying nothing but a number
struct marker_category {
    using category = marker_category;
};

template<int N>
struct marker final : marker_category {
    template<class Registry>
    struct fn {};
};

struct animals : default_registry::with<marker<1>> {};
struct vehicles : default_registry::with<marker<2>> {};

static_assert(!std::is_same_v<animals::registry_type, vehicles::registry_type>);
// end::distinct[]

} // namespace distinct_policies

auto main() -> int {
    // the shared pair reach one state, the distinct pair two
    assert(same_policies::animals::id() == same_policies::vehicles::id());
    assert(
        distinct_policies::animals::id() != distinct_policies::vehicles::id());

    return 0;
}
