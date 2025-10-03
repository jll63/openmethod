// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// tiger.cpp
#include "animals.hpp"

struct Tiger : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Tiger, Carnivore);

extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
auto make_tiger() -> Animal* {
    return new Tiger;
}
}

// end::dl_shared[]
