// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md
// section 16. MSVC x64 only; not built by the CMake harness.
//
//   cl /nologo /std:c++17 /O2 /EHsc /W4 msvc_direct_rtti.cpp
//
// Section 15.2 found that MSVC, unlike GCC and clang, does not inline fetching
// an object's type id: every dispatch begins with a call to __RTtypeid. This
// asks whether the fetch can be inlined by hand, and what it is worth.
//
// MSVC reaches RTTI through a "complete object locator" stored one word before
// the v-table. On x64 the locator's fields are image-relative, so the image
// base has to be recovered from the locator's own address before the type
// descriptor - which is what `&typeid(x)` returns - can be found. None of this
// is documented; it is what the CRT's own __RTtypeid does.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <typeinfo>
#include <utility>
#include <vector>

#if !defined(_MSC_VER) || !defined(_M_X64)
#error "msvc_direct_rtti is MSVC x64 only"
#endif

namespace {

// RTTICompleteObjectLocator, x64 shape: `signature` is 1 and `self` is
// present, which is what makes the image base recoverable. On x86 the
// signature is 0, there is no `self`, and the other fields are real pointers.
struct complete_object_locator {
    std::uint32_t signature;
    std::uint32_t offset;
    std::uint32_t cd_offset;
    std::uint32_t type_descriptor;
    std::uint32_t class_hierarchy;
    std::uint32_t self;
};

// &typeid(obj), for an object whose first word is a v-table pointer.
__forceinline auto direct_type_id(const void* obj) -> const void* {
    auto vptr = *reinterpret_cast<const std::uint8_t* const*>(obj);
    auto col = *reinterpret_cast<const complete_object_locator* const*>(
        vptr - sizeof(void*));

    return reinterpret_cast<const std::uint8_t*>(col) - col->self +
        col->type_descriptor;
}

// ---------------------------------------------------------------------------
// Does it agree with typeid?
// ---------------------------------------------------------------------------

struct A {
    virtual ~A() = default;
    virtual void f() {}
};

struct B {
    virtual ~B() = default;
    virtual void g() {}
};

struct S : A {};
struct M : A, B {};
struct V : virtual A {};
struct VV : V, B {};

int failures = 0;

template<class T>
void check(const char* what, const T& obj, bool expected) {
    auto want = static_cast<const void*>(&typeid(obj));
    auto got = direct_type_id(&obj);
    bool agrees = want == got;
    failures += agrees != expected;
    std::printf(
        "%-32s %-8s %s\n", what, agrees ? "agrees" : "WRONG",
        agrees == expected ? "" : "  <-- not what was expected");
}

void verify() {
    S s;
    M m;
    V v;
    VV vv;

    // Every shape whose leading word is a v-table pointer.
    check("S as S", s, true);
    check("S as A&", static_cast<const A&>(s), true);
    check("M as M", m, true);
    check("M as A& (primary base)", static_cast<const A&>(m), true);
    check("M as B& (secondary base)", static_cast<const B&>(m), true);
    check("V as A& (virtual base)", static_cast<const A&>(v), true);
    check("VV as VV", vv, true);
    check("VV as A&", static_cast<const A&>(vv), true);
    check("VV as B&", static_cast<const B&>(vv), true);

    // And the shapes whose leading word is a *virtual base table* pointer,
    // where the read lands on the wrong word. It does not crash: it produces
    // a plausible address that is not a type descriptor.
    check("V as V (has a virtual base)", v, false);
    check("VV as V&", static_cast<const V&>(vv), false);
}

// ---------------------------------------------------------------------------
// What is it worth, in the shape the dispatch uses it?
// ---------------------------------------------------------------------------

struct base {
    virtual ~base() = default;
};

template<std::size_t I>
struct node : node<(I - 1) / 2> {};

template<>
struct node<0> : base {};

constexpr std::size_t num_classes = 64;
constexpr std::size_t num_objects = 4096;
constexpr std::size_t laps = 200;

std::uint64_t mult = 0x9e3779b97f4a7c15ull;
std::size_t shift = 64 - 10;
std::vector<std::size_t> table(std::size_t(1) << 10, 0);

using factory = std::unique_ptr<base> (*)();

template<std::size_t I>
auto make() -> std::unique_ptr<base> {
    return std::make_unique<node<I>>();
}

template<std::size_t... I>
auto factories(std::index_sequence<I...>) -> std::vector<factory> {
    return {&make<I>...};
}

// The type id, hashed and used to index a table: everything a dispatch does
// before the indirect call.
template<bool Direct>
__declspec(noinline) auto
sweep(const std::vector<std::unique_ptr<base>>& objects) -> std::size_t {
    std::size_t sum = 0;

    for (std::size_t lap = 0; lap != laps; ++lap) {
        for (const auto& object : objects) {
            auto type = Direct
                ? direct_type_id(object.get())
                : static_cast<const void*>(&typeid(*object));
            sum +=
                table[(mult * reinterpret_cast<std::uintptr_t>(type)) >> shift];
        }
    }

    return sum;
}

template<bool Direct>
auto median_ns(const std::vector<std::unique_ptr<base>>& objects) -> double {
    std::vector<double> samples;

    for (int run = 0; run != 15; ++run) {
        auto start = std::chrono::steady_clock::now();
        auto result = sweep<Direct>(objects);
        auto stop = std::chrono::steady_clock::now();
        volatile std::size_t sink = result;
        (void)sink;
        samples.push_back(
            std::chrono::duration<double, std::nano>(stop - start).count() /
            double(laps * objects.size()));
    }

    std::sort(samples.begin(), samples.end());

    return samples[samples.size() / 2];
}

} // namespace

int main() {
    verify();

    if (failures != 0) {
        std::printf("\n%d shape(s) behaved unexpectedly\n", failures);

        return 1;
    }

    std::printf("\nevery shape as expected\n\n");

    auto make_one = factories(std::make_index_sequence<num_classes>{});
    std::vector<std::unique_ptr<base>> objects;

    for (std::size_t i = 0; i != num_objects; ++i) {
        objects.push_back(make_one[i % num_classes]());
    }

    std::printf("ns per fetch, %zu objects, %zu classes\n", num_objects,
                num_classes);

    for (int i = 0; i != 3; ++i) {
        std::printf(
            "  typeid %.3f    direct %.3f\n", median_ns<false>(objects),
            median_ns<true>(objects));
    }

    return 0;
}
