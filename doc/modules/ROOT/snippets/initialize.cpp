// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

namespace bom = boost::openmethod;

struct Animal {
    virtual ~Animal() = default;
};
struct Cat : Animal {};
struct Dog : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog);

BOOST_OPENMETHOD(trick, (bom::virtual_ptr<Animal>), std::string);

BOOST_OPENMETHOD_OVERRIDE(trick, (bom::virtual_ptr<Animal>), std::string) {
    return "stare";
}

BOOST_OPENMETHOD_OVERRIDE(trick, (bom::virtual_ptr<Dog>), std::string) {
    return "spin";
}

BOOST_AUTO_TEST_CASE(initialize_report) {
    // tag::report[]
    auto report = bom::initialize(bom::trace::from_env()).report;

    if (report.not_implemented != 0 || report.ambiguous) {
        std::cerr << "some methods are ambiguous or not implemented for "
                     "some combinations of virtual arguments\n"
                     "set BOOST_OPENMETHOD_TRACE=1 to troubleshoot\n";
        exit(1);
    }
    // end::report[]

    BOOST_TEST(report.not_implemented == 0);
    BOOST_TEST(report.ambiguous == 0);

    Dog snoopy;
    BOOST_TEST(trick(bom::virtual_ptr<Animal>(snoopy)) == "spin");
}
