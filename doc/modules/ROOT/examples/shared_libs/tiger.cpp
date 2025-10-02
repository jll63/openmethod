// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::dl_shared[]
// dl_shared.cpp

#include "animals.hpp"

#include <boost/dll/alias.hpp>

struct Tiger : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Tiger, Carnivore);

extern "C" auto tiger_factory() -> Tiger* {
    return new Tiger;
}

BOOST_DLL_ALIAS(tiger_factory, make_tiger)

// end::dl_shared[]
