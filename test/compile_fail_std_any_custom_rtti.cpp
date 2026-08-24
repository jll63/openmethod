// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: requires standard RTTI

#include <any>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_any.hpp>

using namespace boost::openmethod;

template<typename T>
struct type_tag {
    static constexpr char id = 0;
};

// A complete rtti policy that identifies classes by the address of a per-class
// static variable, rather than by `&typeid(T)`. Nothing else in the library
// objects to it - only the `any` interop does.
struct custom_rtti : policies::rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = false;

        template<typename T>
        static auto static_type() -> type_id {
            return &type_tag<T>::id;
        }

        template<typename T>
        static auto dynamic_type(const T&) -> type_id {
            return &type_tag<T>::id;
        }
    };
};

struct custom_rtti_registry
    : default_registry::with<custom_rtti>::without<policies::type_hash> {};

struct Dog {
    std::string name;
};

// Dispatching on a `std::any` keys on the `std::type_info` returned by
// `std::any::type()`, so the registry's rtti policy must identify classes the
// same way. This one does not: the lookup key would be meaningless, and
// `type_id` being `const void*`, the call would otherwise compile silently.
BOOST_OPENMETHOD(
    name, (virtual_<const std::any&>), std::string, custom_rtti_registry);

int main() {
    // Call the method: declaring it is not enough to instantiate it on
    // every compiler, and the guard lives in `virtual_traits::vptr`.
    std::any dog = Dog{"Snoopy"};
    return name(dog).size();
}
