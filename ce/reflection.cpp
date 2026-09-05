#include <iostream>
#include <vector>
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

struct Animal {
    const char* name;
    Animal(const char* name) : name(name) {
    }
    virtual ~Animal() {
    }
};

struct Dog : Animal {
    using Animal::Animal;
};

struct Cat : Animal {
    using Animal::Animal;
};

// Not named in any method, overrider, or BOOST_OPENMETHOD_CLASSES. But the scan
// finds it.
struct Bulldog : Dog {
    using Dog::Dog;
};

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Cat> animal, std::ostream& os), void) {
    os << animal->name << " hisses.\n";
}

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog> animal, std::ostream& os), void) {
    os << animal->name << " barks.\n";
}

void poke_animals(
    const std::vector<virtual_ptr<Animal>>& animals, std::ostream& os) {
    for (auto animal : animals) {
        poke(animal, os);
    }
}

auto main() -> int {
    boost::openmethod::initialize();

    Dog snoopy{"Snoopy"};
    Cat felix{"Felix"};
    Bulldog hector{"Hector"};
    std::vector<virtual_ptr<Animal>> animals = {snoopy, felix, hector};

    poke_animals(animals, std::cout);
}

// scan for open-methods and register classes used in virtual parameters
BOOST_OPENMETHOD_REGISTER_CLASSES();

// core API - no macros:
// inline /* or static */ boost::openmethod::register_classes<> register_my_classes;
