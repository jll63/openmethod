// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <iostream>

using namespace boost::openmethod::aliases;

class Animal {
  public:
    virtual ~Animal() {
    }
};

class Dog : public Animal {};
class Bulldog : public Dog {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog);

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog> /*dog*/, std::ostream& os), void) {
    os << "bark\n";
}

int main() {
    boost::openmethod::initialize();

    std::unique_ptr<Animal> hector(new Bulldog());

    poke(*hector, std::cout);
}
// end::content[]
