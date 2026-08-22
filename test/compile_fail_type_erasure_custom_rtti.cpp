// Copyright (c) 2018-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>

#include <boost/mpl/vector.hpp>
#include <boost/type_erasure/builtin.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_type_erasure.hpp>

namespace te = boost::type_erasure;
using namespace boost::openmethod;

template<typename T>
struct type_tag {
    static constexpr char id = 0;
};

// A complete rtti policy that identifies classes by the address of a per-class
// static variable, rather than by `&typeid(T)`. Nothing else in the library
// objects to it - only the type_erasure interop does.
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

using Concept =
    boost::mpl::vector<te::copy_constructible<>, te::typeid_<>, te::relaxed>;
using erased = te::any<Concept>;

struct Dog {
    std::string name;
};

// Dispatching on a type_erasure::any keys on the `std::type_info` returned by
// `boost::type_erasure::typeid_of`, so the registry's rtti policy must identify
// classes the same way. This one does not: the lookup key would be meaningless,
// and `type_id` being `const void*`, the call would otherwise compile silently.
BOOST_OPENMETHOD(
    name, (virtual_<const erased&>), std::string, custom_rtti_registry);

int main() {
    // Call the method: declaring it is not enough to instantiate it on
    // every compiler, and the guard lives in `virtual_traits::vptr`.
    erased dog = Dog{"Snoopy"};
    return name(dog).size();
}
