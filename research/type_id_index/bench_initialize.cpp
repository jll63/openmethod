// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.
//
// Initialize-time arm: how long a type_hash / vptr policy takes to build its
// tables, as a function of the number of type ids.
//
// The policies are driven directly, through a stand-in for the
// InitializeContext blueprint, rather than through initialize(): that isolates
// the policy from the rest of the compiler, and lets the type ids be *chosen*
// so the sweep can replay a measured address distribution instead of whatever
// this program's own classes happen to get.
//
// The type ids handed to the policies are fabricated addresses. Neither
// fast_perfect_hash nor the prototypes dereference a type id - they only cast
// it to an integer - but the trace path would (it calls rtti::type_name), so
// this program must never pass a `trace` option, and the registry it uses must
// never be handed to boost::openmethod::initialize().

#include "registry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace bom = boost::openmethod;

namespace {

// A stand-in for InitializeContext: the policies under measurement use only
// classes_begin/classes_end and has_option.
struct fake_class_view {
    const bom::type_id* first;
    const bom::type_id* last;

    auto type_id_begin() const {
        return first;
    }

    auto type_id_end() const {
        return last;
    }

    auto vptr() const -> bom::vptr_type {
        return nullptr;
    }

    auto static_vptr() const -> const bom::vptr_type* {
        return nullptr;
    }
};

struct fake_context {
    template<class Option>
    static constexpr bool has_option = false;

    std::vector<fake_class_view> views;

    auto classes_begin() const {
        return views.begin();
    }

    auto classes_end() const {
        return views.end();
    }
};

auto make_context(const std::vector<bom::type_id>& type_ids) -> fake_context {
    fake_context ctx;
    ctx.views.reserve(type_ids.size());

    for (const auto& type : type_ids) {
        ctx.views.push_back(fake_class_view{&type, &type + 1});
    }

    return ctx;
}

// Synthetic type ids: `n` addresses drawn from a span `n * stride` wide, on a
// 16-byte grid, which is the coarsest grid the ABI guarantees.
auto synthetic_type_ids(std::size_t n, std::size_t stride, std::uint64_t seed)
    -> std::vector<bom::type_id> {
    std::mt19937_64 rnd(seed);

    // Gaps in 16-byte slots, averaging `stride` bytes: distinct and sorted by
    // construction, which is what the addresses of type_info records are.
    const std::uint64_t max_gap = std::max<std::uint64_t>(stride / 8, 2) - 1;
    std::uniform_int_distribution<std::uint64_t> dist(1, max_gap);

    // A plausible shared-library base.
    std::uint64_t address = 0x00007f0000000000ull;
    std::vector<bom::type_id> type_ids;
    type_ids.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        type_ids.push_back(reinterpret_cast<bom::type_id>(address));
        address += dist(rnd) * 16;
    }

    return type_ids;
}

auto read_type_ids(const std::string& path) -> std::vector<bom::type_id> {
    std::ifstream in(path);

    if (!in) {
        std::cerr << "cannot read " << path << "\n";
        std::exit(1);
    }

    std::vector<bom::type_id> type_ids;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        type_ids.push_back(
            reinterpret_cast<bom::type_id>(std::stoull(line, nullptr, 16)));
    }

    return type_ids;
}

template<class Fn>
auto median_ns(Fn fn, int runs) -> double {
    std::vector<double> samples;
    samples.reserve(runs);

    for (int run = 0; run < runs; ++run) {
        auto start = std::chrono::steady_clock::now();
        fn();
        auto stop = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::nano>(stop - start).count());
    }

    std::sort(samples.begin(), samples.end());

    return samples[samples.size() / 2];
}

using registry = research_registry;
using type_hash = registry::policy<bom::policies::type_hash>;
using vptr = registry::policy<bom::policies::vptr>;

// Templates, not `if constexpr` inside `measure`: a discarded statement is
// only left uninstantiated when it is *dependent*, and the `direct` variant
// has no type_hash policy at all - the alias is `void` there, so
// `type_hash::initialize` would have to parse in a non-template context.
template<class TypeHash, class Context>
auto time_hash_initialize(const Context& ctx, int runs) -> double {
    if constexpr (std::is_same_v<TypeHash, void>) {
        (void)ctx;
        (void)runs;

        return 0;
    } else {
        return median_ns(
            [&] { TypeHash::initialize(ctx, std::tuple<>{}); }, runs);
    }
}

// The number of slots vptr_vector will allocate: 8 bytes each, and the bulk
// of what a type_hash policy costs. The `direct` variant has no type_hash
// policy, and folds the base into the table's address instead.
template<class TypeHash>
auto hash_slots() -> std::size_t {
    if constexpr (std::is_same_v<TypeHash, void>) {
        return 0;
    } else {
        return TypeHash::hash_range().second + 1;
    }
}

// Bytes a policy holds beyond the v-table table itself. Only chd_index has
// any: its pilot array.
template<class, class = void>
struct has_aux_bytes : std::false_type {};

template<class TypeHash>
struct has_aux_bytes<TypeHash, std::void_t<decltype(TypeHash::aux_bytes())>> :
    std::true_type {};

template<class TypeHash>
auto aux_bytes() -> std::size_t {
    if constexpr (std::is_same_v<TypeHash, void>) {
        return 0;
    } else if constexpr (has_aux_bytes<TypeHash>::value) {
        return TypeHash::aux_bytes();
    } else {
        return 0;
    }
}

template<class TypeHash>
auto finalize_hash() -> void {
    if constexpr (!std::is_same_v<TypeHash, void>) {
        TypeHash::finalize(std::tuple<>{});
    }
}

void measure(
    const char* label, const std::vector<bom::type_id>& type_ids, int runs) {
    auto ctx = make_context(type_ids);
    const std::tuple<> no_options;

    auto hash_ns = time_hash_initialize<type_hash>(ctx, runs);
    auto vptr_ns = median_ns([&] { vptr::initialize(ctx, no_options); }, runs);

    std::cout << RESEARCH_POLICY_NAME << "," << label << "," << type_ids.size()
              << "," << hash_ns << "," << vptr_ns << ","
              << hash_slots<type_hash>() << "," << aux_bytes<type_hash>()
              << "\n";

    vptr::finalize(no_options);
    finalize_hash<type_hash>();
}

} // namespace

int main(int argc, char** argv) {
    int runs = 3;
    std::cout << "variant,input,n,hash_init_ns,vptr_init_ns,slots,aux_bytes\n";

    if (argc > 1) {
        // Replay measured distributions: one file of hex addresses each.
        for (int i = 1; i < argc; ++i) {
            measure(argv[i], read_type_ids(argv[i]), runs);
        }

        return 0;
    }

    // stride is in bytes of address span per type id: 16 is a densely packed
    // module, 1024 is what a real library's interleaved layout looks like.
    for (std::size_t stride :
         {std::size_t(16), std::size_t(64), std::size_t(256)}) {
        for (std::size_t bits = 4; bits <= 16; ++bits) {
            auto n = std::size_t(1) << bits;
            auto label = "synthetic-stride" + std::to_string(stride);
            measure(
                label.c_str(), synthetic_type_ids(n, stride, 20260906), runs);
        }
    }

    return 0;
}
