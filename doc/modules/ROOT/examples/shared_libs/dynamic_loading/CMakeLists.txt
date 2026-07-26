# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

# Dynamic loading, direct virtual_ptrs. The executable owns the registry state
# (main.cpp emits BOOST_OPENMETHOD_EXPORT_REGISTRY); the
# runtime-loaded library imports it, so it links against the exe - the reverse
# of the usual direction. On Windows this needs the exe's import library,
# generated because of ENABLE_EXPORTS; on other platforms ENABLE_EXPORTS adds
# -rdynamic (or equivalent).

add_executable(boost_openmethod-dynamic main.cpp)
set_target_properties(boost_openmethod-dynamic PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(boost_openmethod-dynamic Boost::openmethod Boost::dll)

add_library(boost_openmethod-shared SHARED extensions.cpp)
target_link_libraries(boost_openmethod-shared
    PRIVATE Boost::openmethod boost_openmethod-dynamic)

# Co-locate the library with the exe on all platforms. On Windows a DLL is a
# runtime output and already lands next to the exe, but elsewhere a shared
# library is a *library* output and would stay in this subdirectory, where the
# test below cannot find it.
set_target_properties(boost_openmethod-shared PROPERTIES
    ENABLE_EXPORTS ON
    LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-dynamic>)

# Run from the directory holding the freshly-built exe and its co-located DLL:
# main.cpp loads the DLL by relative name, which boost::dll resolves against the
# working directory.
add_test(
    NAME boost_openmethod-dynamic_shared
    COMMAND boost_openmethod-dynamic
    WORKING_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-dynamic>)

if (TARGET tests)
    add_dependencies(tests
        boost_openmethod-dynamic boost_openmethod-shared)
endif()
