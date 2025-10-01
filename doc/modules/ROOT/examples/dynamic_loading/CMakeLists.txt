# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Building dynamic loading example")

add_executable(nature nature.cpp)
set_target_properties(nature PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(nature Boost::openmethod Boost::dll)

add_library(jungle SHARED jungle.cpp)
get_target_property(BOOST_OPENMETHOD_INCLUDE_DIRS Boost::openmethod INTERFACE_INCLUDE_DIRECTORIES)
target_link_libraries(jungle Boost::openmethod)
set_target_properties(jungle PROPERTIES ENABLE_EXPORTS ON)

add_dependencies(nature jungle)
add_test(NAME dlopen COMMAND nature)
