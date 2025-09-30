# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Building dynamic loading example")
add_executable(nature nature.cpp)
add_library(jungle SHARED jungle.cpp)
get_target_property(BOOST_OPENMETHOD_INCLUDE_DIRS Boost::openmethod INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(jungle PUBLIC ${BOOST_OPENMETHOD_INCLUDE_DIRS})
add_dependencies(nature jungle)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(nature PROPERTIES LINK_FLAGS "-rdynamic")
endif()
target_link_libraries(nature Boost::openmethod dl Boost::dll)
target_link_libraries(jungle Boost::openmethod)
add_test(NAME dlopen COMMAND nature)
