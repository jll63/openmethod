// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Probe for C++26 reflection (P2996), compiled with -freflection. See
// ../Jamfile.

#include <meta>

struct Base {};
struct Derived : Base {};

consteval auto count() -> int {
    return static_cast<int>(
        std::meta::bases_of(^^Derived, std::meta::access_context::unchecked())
            .size());
}

static_assert(count() == 1);
