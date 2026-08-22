# Copyright (c) 2018-2027 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

# default_registry shared across an implicitly linked library, using the
# BOOST_OPENMETHOD_{EXPORT,IMPORT}_REGISTRY macros.

set(lib boost_openmethod-isl_default_lib)
set(exe boost_openmethod-test_implicit_shared_libraries_default)

# lib.cpp emits the one definition of the registry state: this library owns
# it, and the executable imports it.
add_library(${lib} SHARED lib.cpp)
target_link_libraries(${lib} PRIVATE Boost::openmethod)

add_executable(${exe} main.cpp)
target_link_libraries(${exe}
    PRIVATE Boost::openmethod Boost::unit_test_framework ${lib})

# Co-locate the library with the executable so it is found at run time on
# Windows, where there is no rpath. On ELF and Mach-O, CMake's default
# build-tree RPATH already resolves an implicitly linked library, but a shared
# library is a *library* output there and would otherwise stay in this
# subdirectory.
set_target_properties(${lib} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${exe}>
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${exe}>)

boost_openmethod_add_test(${exe})

if (TARGET tests)
    add_dependencies(tests ${exe} ${lib})
endif()
