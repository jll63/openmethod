# Copyright (c) 2017-2026 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

# Dynamic loading with indirect virtual_ptrs, which stay valid across the
# initialize() call that follows loading. Same layout as dynamic_loading; the
# registry is selected in animals.hpp, which defines
# BOOST_OPENMETHOD_DEFAULT_REGISTRY to indirect_registry, so no target here
# needs a compile definition.

add_executable(boost_openmethod-indirect main.cpp)
set_target_properties(boost_openmethod-indirect PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(boost_openmethod-indirect
    PRIVATE Boost::openmethod Boost::dll)

add_library(boost_openmethod-indirect_shared SHARED extensions.cpp)
target_link_libraries(boost_openmethod-indirect_shared
    PRIVATE Boost::openmethod boost_openmethod-indirect)

set_target_properties(boost_openmethod-indirect_shared PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-indirect>
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-indirect>)

add_test(
    NAME boost_openmethod-indirect
    COMMAND boost_openmethod-indirect
    WORKING_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-indirect>)

if (TARGET tests)
    add_dependencies(tests
        boost_openmethod-indirect boost_openmethod-indirect_shared)
endif()
