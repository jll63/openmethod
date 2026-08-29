// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>

#include <boost/mpl/vector.hpp>
#include <boost/type_erasure/builtin.hpp>

#include <boost/openmethod/policies/static_rtti.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

namespace te = boost::type_erasure;
using namespace boost::openmethod;

// The `openmethod_vptr` concept takes the v-table pointer from the `any`'s own
// dispatch table, so it does not go through `typeid_of`, and the `std_rtti`
// requirement that the `typeid_of`-based traits assert does not apply. The
// registry then needs an rtti policy only for the static type identification
// `initialize()` performs - `static_rtti` suffices - and neither a `vptr`
// policy nor the `type_hash` one would depend on.
struct minimal_registry : registry<policies::static_rtti> {};

struct Dog {
    std::string name;
};

struct Cat {
    std::string name;
};

struct Dispatchable :
    boost::mpl::vector<
        te::copy_constructible<>, te::relaxed,
        openmethod_vptr<Dispatchable, minimal_registry>> {};

using erased = te::any<Dispatchable>;

// Binding a value to the `any` registers its type.

BOOST_OPENMETHOD(
    name, (virtual_<const erased&>), std::string, minimal_registry);

BOOST_OPENMETHOD_OVERRIDE(name, (const Dog& dog), std::string) {
    return dog.name + " the dog";
}

BOOST_OPENMETHOD_OVERRIDE(name, (const Cat& cat), std::string) {
    return cat.name + " the cat";
}

BOOST_AUTO_TEST_CASE(type_erasure_openmethod_vptr_needs_no_std_rtti) {
    initialize<minimal_registry>();

    const erased spot(Dog{"Spot"});
    const erased tom(Cat{"Tom"});

    BOOST_TEST(name(spot) == "Spot the dog");
    BOOST_TEST(name(tom) == "Tom the cat");
}
