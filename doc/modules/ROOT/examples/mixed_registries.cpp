// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <type_traits>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE mixed_registries
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

// tag::nodes[]
struct Node {
    explicit Node(unsigned type) : type(type) {
    }

    unsigned type;
    static constexpr unsigned static_type = 1;
};

struct Number : Node {
    static constexpr unsigned static_type = 2;

    explicit Number(int value) : Node(static_type), value(value) {
    }

    int value;
};

struct Plus : Node {
    static constexpr unsigned static_type = 3;

    Plus(const Node& left, const Node& right) :
        Node(static_type), left(left), right(right) {
    }

    const Node& left;
    const Node& right;
};

struct node_rtti : policies::rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Node, T>;

        template<typename T>
        static auto static_type() -> type_id {
            if constexpr (is_polymorphic<T>) {
                return reinterpret_cast<type_id>(T::static_type);
            } else {
                return nullptr;
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) -> type_id {
            if constexpr (is_polymorphic<T>) {
                return reinterpret_cast<type_id>(obj.type);
            } else {
                return nullptr;
            }
        }
    };
};

struct node_registry : registry<node_rtti, policies::vptr_vector> {};

BOOST_OPENMETHOD_CLASSES(Node, Number, Plus, node_registry);
// end::nodes[]

// tag::formats[]
struct Format {
    virtual ~Format() = default;
};

struct Postfix : Format {};
struct Infix : Format {};

BOOST_OPENMETHOD_CLASSES(Format, Postfix, Infix);
// end::formats[]

// tag::method[]
BOOST_OPENMETHOD(
    render, (virtual_<const Node&, node_registry>, virtual_<const Format&>),
    std::string, default_registry);

BOOST_OPENMETHOD_OVERRIDE(
    render, (const Number& number, const Format&), std::string) {
    return std::to_string(number.value);
}

BOOST_OPENMETHOD_OVERRIDE(
    render, (const Plus& plus, const Postfix& format), std::string) {
    return render(plus.left, format) + " " + render(plus.right, format) + " +";
}

BOOST_OPENMETHOD_OVERRIDE(
    render, (const Plus& plus, const Infix& format), std::string) {
    return "(" + render(plus.left, format) + " + " +
        render(plus.right, format) + ")";
}
// end::method[]

BOOST_AUTO_TEST_CASE(mixed_registries) {
    // tag::initialize[]
    initialize<node_registry>();
    initialize();
    // end::initialize[]

    // tag::call[]
    Number one(1), two(2), three(3);
    Plus sum(one, two), total(sum, three);

    BOOST_TEST(render(total, Postfix()) == "1 2 + 3 +");
    BOOST_TEST(render(total, Infix()) == "((1 + 2) + 3)");
    // end::call[]
}
