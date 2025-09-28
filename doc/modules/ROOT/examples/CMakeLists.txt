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

foreach(cpp ${cpp_files})
    if(cpp MATCHES "dl_.*\\.cpp")
        continue()
    endif()

    cmake_path(REMOVE_EXTENSION cpp LAST_ONLY OUTPUT_VARIABLE stem)
    string(REGEX REPLACE ".*/" "" stem ${stem})
    add_executable(${stem} ${cpp})
    target_link_libraries(${stem} PUBLIC Boost::openmethod Boost::unit_test_framework)
    add_test(NAME ${stem} COMMAND ${stem})
    add_dependencies(tests ${stem})
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "Building dlopen example")
  add_executable(dl_main dl_main.cpp)
  add_library(dl_shared SHARED dl_shared.cpp)
  get_target_property(BOOST_OPENMETHOD_INCLUDE_DIRS Boost::openmethod INTERFACE_INCLUDE_DIRECTORIES)
  target_include_directories(dl_shared PUBLIC ${BOOST_OPENMETHOD_INCLUDE_DIRS})
  add_dependencies(dl_main dl_shared)
  set_target_properties(dl_main PROPERTIES LINK_FLAGS "-Wl,-export-dynamic")
  target_link_libraries(dl_main Boost::openmethod dl)
  target_link_libraries(dl_shared Boost::openmethod)
  add_test(NAME dlopen COMMAND dl_main)
endif()

add_subdirectory(headers_namespaces)
