# Copyright (c) 2018-2027 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Boost.OpenMethod: building shared library examples")

add_compile_definitions(BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)

# One self-contained example per directory. Each has its own animals.hpp,
# main.cpp and extensions.cpp, so every file can be read top to bottom: the
# export/import macro each one uses is spelled out unconditionally, rather than
# depending on which other file textually included it.
add_subdirectory(implicit_linking)
add_subdirectory(dynamic_loading)
add_subdirectory(indirect_vptr)
