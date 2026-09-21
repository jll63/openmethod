// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Probe for BMI2's parallel bit extract, compiled with -mbmi2. See ../Jamfile,
// and boost/openmethod/policies/minimal_cover_hash.hpp, which is the only part
// of the library that needs the instruction.
//
// It tests the header's own feature macro rather than the intrinsic directly:
// what the test suite needs to know is whether that header will let the policy
// be used, which is a slightly narrower question than whether some spelling of
// pext compiles.
//
// The probe runs, it does not just compile. GCC and clang accept -mbmi2, and
// therefore emit `pext`, whenever the *toolchain* supports the extension -
// that says nothing about whether the machine actually running the build
// implements it in hardware. A CI runner can advertise a BMI2-capable
// compiler while executing on pre-Haswell (or virtualized/feature-masked)
// silicon, where the instruction traps with SIGILL. ../Jamfile turns this
// into a `run` target for exactly that reason: UPDATE_NOW executes the
// program, and a trap fails the check the same way a compile error would.
//
// `argc` keeps the operands from being folded to a compile-time constant, so
// optimization cannot turn the one `pext` this program executes into a
// no-op.

#include <boost/openmethod/policies/minimal_cover_hash.hpp>

#include <cstdint>

static_assert(BOOST_OPENMETHOD_HAS_PEXT);

auto main(int argc, char**) -> int {
    auto value = std::uint64_t(argc) | (std::uint64_t(1) << 63);
    auto mask = std::uint64_t(argc) * 0x5555555555555555ull;

    return boost::openmethod::detail::pext64(value, mask) == 0xdeadbeef ? 1 : 0;
}
