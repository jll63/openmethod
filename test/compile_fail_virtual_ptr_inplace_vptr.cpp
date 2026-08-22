// Copyright (c) 2018-2027 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>

using namespace boost::openmethod;

struct Animal : inplace_vptr_base<Animal> {
    virtual ~Animal() = default;
};

// An object with a boost_openmethod_vptr overload carries its own v-table
// pointer; wrapping it in a virtual_ptr is rejected at compile time.

int main() {
    Animal animal;
    virtual_ptr<Animal> p(animal);
    return 0;
}
