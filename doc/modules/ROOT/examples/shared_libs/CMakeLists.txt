# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Building dynamic_shared loading examples")

# ------------------------------------------------------------------------------
# static linking

add_library(shared SHARED overriders.cpp tiger.cpp)
target_link_libraries(shared Boost::openmethod Boost::dll)
set_target_properties(shared PROPERTIES ENABLE_EXPORTS ON)

add_executable(static static_main.cpp)
target_link_libraries(static Boost::openmethod Boost::dll shared)
add_test(NAME static_shared COMMAND static)

# ------------------------------------------------------------------------------
# dynamic loading, direct virtual_ptrs

add_executable(dynamic dynamic_main.cpp)
set_target_properties(dynamic PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(dynamic Boost::openmethod Boost::dll)
add_dependencies(dynamic shared)
add_test(NAME dynamic_shared COMMAND dynamic)

# ------------------------------------------------------------------------------
# dynamic loading, indirect virtual_ptrs

add_library(indirect_shared SHARED indirect_overriders.cpp indirect_tiger.cpp)
target_compile_definitions(
    indirect_shared PUBLIC BOOST_OPENMETHOD_DEFAULT_REGISTRY=indirect_registry)
target_link_libraries(indirect_shared Boost::openmethod Boost::dll)
set_target_properties(indirect_shared PROPERTIES ENABLE_EXPORTS ON)

add_executable(indirect indirect_main.cpp)
target_compile_definitions(
    indirect PUBLIC BOOST_OPENMETHOD_DEFAULT_REGISTRY=indirect_registry)
set_target_properties(indirect PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(indirect Boost::openmethod Boost::dll)
add_dependencies(indirect indirect_shared)
add_test(NAME indirect_shared COMMAND indirect)
