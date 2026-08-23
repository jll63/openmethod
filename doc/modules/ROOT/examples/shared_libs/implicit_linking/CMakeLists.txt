# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

# Implicit (load-time) linking. The shared library owns the registry state
# (extensions.cpp emits the one definition of the state); the
# executable imports it. This keeps the link dependency in the usual direction
# on Windows: the exe links the library's import library.

add_library(boost_openmethod-static_shared SHARED extensions.cpp)
target_link_libraries(boost_openmethod-static_shared PRIVATE Boost::openmethod)

add_executable(boost_openmethod-static main.cpp)
target_link_libraries(boost_openmethod-static
    PRIVATE Boost::openmethod boost_openmethod-static_shared)

# Co-locate the library with the exe so it is found at run time on Windows,
# where there is no rpath.
set_target_properties(boost_openmethod-static_shared PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-static>
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:boost_openmethod-static>)

add_test(NAME boost_openmethod-static COMMAND boost_openmethod-static)

# The test runs these, so `tests` - the target the ctest workflow builds - must
# build them; otherwise the test reports "Not Run".
if (TARGET tests)
    add_dependencies(tests
        boost_openmethod-static boost_openmethod-static_shared)
endif()
