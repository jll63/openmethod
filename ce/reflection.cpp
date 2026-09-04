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

// Named nowhere else: no overrider, no BOOST_OPENMETHOD_CLASSES. Only the scan
// finds it.
struct Bulldog : Dog {
    using Dog::Dog;
};

using boost::openmethod::virtual_;

BOOST_OPENMETHOD(poke, (virtual_<Animal&>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat & animal, std::ostream& os), void) {
    os << animal.name << " hisses.\n";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog & animal, std::ostream& os), void) {
    os << animal.name << " barks.\n";
}

BOOST_OPENMETHOD_REGISTER_CLASSES();

void poke_animals(const std::vector<Animal*>& animals, std::ostream& os) {
    for (auto animal : animals) {
        poke(*animal, os);
    }
}

auto main() -> int {
    boost::openmethod::initialize();

    Dog snoopy{"Snoopy"};
    Cat felix{"Felix"};
    Bulldog hector{"Hector"};
    std::vector<Animal*> animals = {&snoopy, &felix, &hector};

    poke_animals(animals, std::cout);
}
