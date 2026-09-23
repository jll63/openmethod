// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Expected diagnostic, as a CMake regex (see CMakeLists.txt).
// expected-error: nothrow move-assignable

#include <stdexcept>
#include <tuple>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using namespace boost::openmethod;

// initialize() saves this policy's state and puts it back, from the
// transaction's destructor, if another policy throws. That destructor runs
// while an exception is in flight and is noexcept by default, so a
// move-assignment that can throw would terminate the program instead of
// letting the original error through. The transaction refuses the policy at
// compile time rather than leave that in the program.
struct throwing_move_policy {
    using category = throwing_move_policy;

    template<class Registry>
    struct fn {
        struct state {
            state() = default;
            state(const state&) = default;
            auto operator=(const state&) -> state& = default;

            state(state&&) {
                throw std::runtime_error("move");
            }

            auto operator=(state&&) -> state& {
                throw std::runtime_error("move");
            }

            int generation = 0;
        };

        template<class Context, class... Options>
        static void initialize(const Context&, const std::tuple<Options...>&) {
            ++Registry::template state<throwing_move_policy>().generation;
        }
    };
};

struct bad_registry : default_registry::with<throwing_move_policy> {};

struct Animal {
    virtual ~Animal() = default;
};

BOOST_OPENMETHOD_REGISTER(use_classes<Animal, bad_registry>);

int main() {
    initialize<bad_registry>();

    return 0;
}
