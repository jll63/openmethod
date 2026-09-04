# Copyright (c) 2017-2026 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Boost.OpenMethod: building Compiler Explorer examples")

file(GLOB cpp_files "*.cpp")

foreach (cpp ${cpp_files})
    get_filename_component(stem ${cpp} NAME_WE)
    set(test_target "boost_openmethod-ce-${stem}")
    add_executable(${test_target} ${cpp})
    boost_openmethod_enable_reflection(${test_target})
    target_link_libraries(${test_target} PRIVATE Boost::openmethod)
    add_test(NAME ${test_target} COMMAND ${test_target})
    add_dependencies(tests ${test_target})
endforeach()
