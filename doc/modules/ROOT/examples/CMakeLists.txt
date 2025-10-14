# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Building examples")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-save-temps -masm=intel)
  endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)
endif()

file(GLOB cpp_files "*.cpp")

foreach (cpp ${cpp_files})
    cmake_path(REMOVE_EXTENSION cpp LAST_ONLY OUTPUT_VARIABLE stem)
    string(REGEX REPLACE ".*/" "" stem ${stem})
    add_executable(${stem} ${cpp})
    target_link_libraries(${stem} PUBLIC Boost::openmethod Boost::unit_test_framework)
    add_test(NAME ${stem} COMMAND ${stem})
    add_dependencies(tests ${stem})
endforeach()

if (NOT WIN32)
  add_subdirectory(shared_libs)
endif()

file(GLOB subdirs "rolex/*")

foreach (subdir ${subdirs})
  string(REGEX REPLACE ".*/" "" subex ${subdir})
  file(GLOB cpp_files "${subdir}/*.cpp")
  add_executable("rolex_${subex}" ${cpp_files})
  target_link_libraries("rolex_${subex}" PUBLIC Boost::openmethod)
  add_test(NAME "rolex_${subex}" COMMAND "rolex_${subex}")
  add_dependencies(tests "rolex_${subex}")
endforeach()
