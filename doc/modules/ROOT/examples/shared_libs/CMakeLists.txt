# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Building shared library examples")

add_compile_definitions(BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)

# ------------------------------------------------------------------------------
# dynamic loading, direct virtual_ptrs

add_executable(dynamic dynamic_main.cpp)
set_target_properties(dynamic PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(dynamic Boost::openmethod Boost::dll)
#add_dependencies(dynamic shared)
add_test(NAME dynamic_shared COMMAND dynamic)

add_library(shared SHARED extensions.cpp)
target_link_libraries(shared PRIVATE Boost::openmethod dynamic)
set_target_properties(shared PROPERTIES ENABLE_EXPORTS ON)

# ------------------------------------------------------------------------------
# static linking

# add_library(shared SHARED extensions.cpp)
# target_link_libraries(shared Boost::openmethod)
# set_target_properties(shared PROPERTIES ENABLE_EXPORTS ON)

# add_executable(static static_main.cpp)
# target_link_libraries(static Boost::openmethod Boost::dll shared)
# add_test(NAME static_shared COMMAND static)

# ------------------------------------------------------------------------------
# dynamic loading, indirect virtual_ptrs

add_library(indirect_shared SHARED indirect_extensions.cpp)
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
if (NOT WIN32)
  add_test(NAME indirect_shared COMMAND indirect)
endif()
