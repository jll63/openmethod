# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Building examples")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-save-temps=obj -masm=intel)
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

function(add_step_by_step dir)
  set(add_test "")
  if(ARGC GREATER 1)
    set(add_test "${ARGV1}")
  else()
    set(add_test "ON")
  endif()
  file(GLOB subdirs "${dir}/*")

  foreach (subdir ${subdirs})
    string(REGEX REPLACE ".*/" "" subex ${subdir})
    file(GLOB cpp_files "${subdir}/*.cpp")
    set(target "${dir}_${subex}")
    add_executable(${target} ${cpp_files})
    target_link_libraries(${target} PUBLIC Boost::openmethod)
    set(output_dir "${CMAKE_BINARY_DIR}/${subdir}")
    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
      LIBRARY_OUTPUT_DIRECTORY "${output_dir}"
      ARCHIVE_OUTPUT_DIRECTORY "${output_dir}"
    )
    if(${add_test})
      add_test(NAME "${target}" COMMAND "${target}")
    endif()
    add_dependencies(tests "${target}")
  endforeach()
endfunction()

add_step_by_step(rolex)
add_step_by_step(ambiguities OFF)
add_step_by_step(core_api)
add_step_by_step(custom_rtti)
add_step_by_step(virtual_ptr_alt)

if (NOT WIN32)
  add_subdirectory(shared_libs)
endif()
