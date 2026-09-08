#
# Research harness for boostorg/openmethod#57 - see TYPE_ID_INDEX.md.
#
# Not part of the library, not built by default, and deliberately absent from
# the Jamfile so CI never sees it. Enable with
# -DBOOST_OPENMETHOD_BUILD_RESEARCH=ON.
#

message(STATUS "Boost.OpenMethod: building the type_id-index research harness")

add_custom_target(research-all)

set(BOOST_OPENMETHOD_RESEARCH_VARIANTS fph index index16 direct pext chd chdmin twolevel twolevel1)

# The pext variant uses BMI2's parallel bit extract. Compiled in only for that
# variant, so the others measure what a stock build would.
function(research_variant_flags target variant)
    if (variant STREQUAL "pext")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
            target_compile_options(${target} PRIVATE /arch:AVX2)
        else()
            target_compile_options(${target} PRIVATE -mbmi2)
        endif()
    endif()
endfunction()

# test_core is deliberately absent: it names default_registry explicitly, three
# dozen times, so force-including a different default registry does not
# reconfigure it, it breaks it. Every test listed here is registry-agnostic.
#
# The whitelisted tests are re-run against each prototype registry by
# force-including registry.hpp, which defines BOOST_OPENMETHOD_DEFAULT_REGISTRY
# before <boost/openmethod.hpp> is seen. No precompiled header may be attached:
# a PCH that already pulled in core.hpp would fix the registry before the
# force-included header can change it (see the comment in test/CMakeLists.txt).
set(BOOST_OPENMETHOD_RESEARCH_TESTS
    test_dispatch_lvalue_refs
    test_dispatch_rvalue_refs
    test_dispatch_pointer
    test_dispatch_multi
    test_dispatch_next_fn
    test_dispatch_across_namespaces
    test_virtual_ptr_dispatch
    test_virtual_ptr_by_ref
    test_rolex
)

function(research_force_include target)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
        target_compile_options(${target} PRIVATE /FI${CMAKE_CURRENT_SOURCE_DIR}/registry.hpp)
    else()
        target_compile_options(${target} PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/registry.hpp)
    endif()
endfunction()

foreach(variant ${BOOST_OPENMETHOD_RESEARCH_VARIANTS})
    string(TOUPPER ${variant} variant_macro)

    foreach(checks OFF ON)
        set(suffix "${variant}")
        if (checks)
            set(suffix "${variant}_checked")
        endif()

        foreach(test ${BOOST_OPENMETHOD_RESEARCH_TESTS})
            set(target "research-${suffix}-${test}")
            add_executable(${target} EXCLUDE_FROM_ALL
                ${PROJECT_SOURCE_DIR}/test/${test}.cpp)
            target_link_libraries(${target}
                PRIVATE Boost::openmethod Boost::unit_test_framework)
            target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/test)
            target_compile_definitions(${target}
                PRIVATE RESEARCH_POLICY=BOOST_OPENMETHOD_RESEARCH_${variant_macro})
            if (checks)
                target_compile_definitions(${target}
                    PRIVATE BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)
            endif()
            research_force_include(${target})
            research_variant_flags(${target} ${variant})
            add_test(NAME ${target} COMMAND ${target})
            add_dependencies(research-all ${target})
        endforeach()

        # Properties of the prototypes themselves.
        set(target "research-${suffix}-check_prototype")
        add_executable(${target} EXCLUDE_FROM_ALL check_prototype.cpp)
        target_link_libraries(${target}
            PRIVATE Boost::openmethod Boost::unit_test_framework)
        target_compile_definitions(${target}
            PRIVATE RESEARCH_POLICY=BOOST_OPENMETHOD_RESEARCH_${variant_macro})
        if (checks)
            target_compile_definitions(${target}
                PRIVATE BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)
        endif()
        research_variant_flags(${target} ${variant})
        add_test(NAME ${target} COMMAND ${target})
        add_dependencies(research-all ${target})
    endforeach()

    # Benchmarks: one executable per variant, no runtime checks.
    foreach(bench bench_dispatch bench_initialize)
        set(target "research-${variant}-${bench}")
        add_executable(${target} EXCLUDE_FROM_ALL ${bench}.cpp)
        target_link_libraries(${target} PRIVATE Boost::openmethod)
        target_compile_definitions(${target}
            PRIVATE RESEARCH_POLICY=BOOST_OPENMETHOD_RESEARCH_${variant_macro})
        research_variant_flags(${target} ${variant})
        add_dependencies(research-all ${target})
    endforeach()
endforeach()

# Disassembly of the dispatch benchmarks: the robust half of the comparison.
find_program(BOOST_OPENMETHOD_OBJDUMP objdump)

if (BOOST_OPENMETHOD_OBJDUMP)
    set(disasm_commands)
    foreach(variant ${BOOST_OPENMETHOD_RESEARCH_VARIANTS})
        list(APPEND disasm_commands
            COMMAND ${CMAKE_COMMAND} -E echo "########## ${variant}"
            COMMAND ${BOOST_OPENMETHOD_OBJDUMP} -dC -M intel --no-show-raw-insn
                $<TARGET_FILE:research-${variant}-bench_dispatch>)
    endforeach()
    add_custom_target(research-disasm ${disasm_commands} VERBATIM)
endif()
