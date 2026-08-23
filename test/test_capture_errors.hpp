// Copyright (c) 2018-2027 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_CAPTURE_ERRORS_HPP
#define BOOST_OPENMETHOD_TEST_CAPTURE_ERRORS_HPP

// Include after the #define that selects the test's registry - which is the
// first line of every test that uses this header, so this is automatic.
#include <boost/openmethod.hpp>

#include <sstream>
#include <variant>

struct capture_output : boost::openmethod::policies::output {
    template<class Registry>
    struct fn {
        inline static std::ostringstream os;
        static auto& stream() {
            return os;
        }
    };
};

template<class Registry>
struct capture_errors {
    using error_handler = typename Registry::error_handler;
    using output = typename Registry::output;

    capture_errors() {
        prev = error_handler::set(
            [this](const typename error_handler::error_variant& error) {
                prev(error);
                std::visit([](auto&& arg) { throw arg; }, error);
            });
    }

    ~capture_errors() {
        error_handler::set(prev);
        output::os.str("");
    }

    auto operator()() const {
        return output::os.str();
    }

    typename error_handler::function_type prev;
};

#endif
