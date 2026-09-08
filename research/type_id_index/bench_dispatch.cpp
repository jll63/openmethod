// Copyright (c) 2017-2026 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// RESEARCH PROTOTYPE - not part of the library's API. See TYPE_ID_INDEX.md.
//
// Dispatch-time arm. Only a call through a plain reference goes through the
// type_id -> index map; a virtual_ptr argument carries its v-table pointer and
// never looks one up. So every loop here calls through `const node_base&`.
//
// Two modes, because the effect under measurement is a couple of cycles:
//
//   latency     each call yields the index of the next object to visit, so the
//               v-table load sits on the dependency chain and its latency
//               cannot be hidden by the out-of-order window;
//   throughput  a plain loop over the array, accumulating.
//
// The wall clock is the noisy half of this comparison. The robust half is the
// generated code - see the `research-disasm` target.

#include "registry.hpp"

#include <boost/openmethod/initialize.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

namespace bom = boost::openmethod;

namespace {

// Keep this small. use_classes relates every pair of registered classes, so
// the compile cost is roughly quadratic: at 512 this translation unit reached
// 23.5 GB resident and 2m37s on GCC 13.3, enough - built in parallel - to fire
// the machine's OOM killer. At 64 it is 0.7 GB and 5 s. The measured dispatch
// cost barely moves with the class count anyway; sweep the table size instead.
constexpr std::size_t num_classes = 64;
constexpr std::size_t num_objects = 4096;

struct node_base {
    virtual ~node_base() = default;
    std::size_t next = 0;
};

// node<I> derives from node<(I-1)/2>: a binary tree of depth log2(I), so the
// hierarchy is not a flat forest and the dispatch tables are not trivial.
template<std::size_t I>
struct node : node<(I - 1) / 2> {};

template<>
struct node<0> : node_base {};

template<class>
struct register_classes_aux;

template<std::size_t... Is>
struct register_classes_aux<std::index_sequence<Is...>> {
    using type = bom::use_classes<node_base, node<Is>...>;
};

static
    typename register_classes_aux<std::make_index_sequence<num_classes>>::type
        register_all;

BOOST_OPENMETHOD(visit, (bom::virtual_<const node_base&>), std::size_t);

using visit_method = BOOST_OPENMETHOD_TYPE(
    visit, (bom::virtual_<const node_base&>), std::size_t);

BOOST_OPENMETHOD_OVERRIDE(visit, (const node_base& n), std::size_t) {
    return n.next;
}

// One overrider per class, so the v-tables differ from one another.
template<std::size_t I>
auto visit_node(const node<I>& n) -> std::size_t {
    return n.next;
}

template<class>
struct register_overriders_aux;

template<std::size_t... Is>
struct register_overriders_aux<std::index_sequence<Is...>> {
    using type = std::tuple<visit_method::override<visit_node<Is>>...>;
};

static typename register_overriders_aux<
    std::make_index_sequence<num_classes>>::type register_overriders;

using factory = std::unique_ptr<node_base> (*)();

template<std::size_t... Is>
auto make_factories(std::index_sequence<Is...>) -> std::vector<factory> {
    return {+[]() -> std::unique_ptr<node_base> {
        return std::make_unique<node<Is>>();
    }...};
}

auto make_objects(std::size_t classes, std::uint64_t seed)
    -> std::vector<std::unique_ptr<node_base>> {
    static const auto factories =
        make_factories(std::make_index_sequence<num_classes>{});

    std::mt19937_64 rnd(seed);
    std::uniform_int_distribution<std::size_t> dist(0, classes - 1);

    std::vector<std::unique_ptr<node_base>> objects;
    objects.reserve(num_objects);

    for (std::size_t i = 0; i < num_objects; ++i) {
        objects.push_back(factories[dist(rnd)]());
    }

    // A random permutation of the object indexes, as a cycle, so the latency
    // loop walks every object exactly once per lap without a predictable
    // stride.
    std::vector<std::size_t> order(num_objects);

    for (std::size_t i = 0; i < num_objects; ++i) {
        order[i] = i;
    }

    std::shuffle(order.begin(), order.end(), rnd);

    for (std::size_t i = 0; i < num_objects; ++i) {
        objects[order[i]]->next = order[(i + 1) % num_objects];
    }

    return objects;
}

// Keep a loop's result alive so the loop itself cannot be optimized away.
// Outside the timed region in every caller, so the sink costs nothing that is
// measured.
template<class T>
void keep_alive(const T& value) {
#if defined(_MSC_VER) && !defined(__clang__)
    _ReadWriteBarrier();
    volatile T sink = value;
    (void)sink;
#else
    asm volatile("" : : "r"(value) : "memory");
#endif
}

template<class Fn>
auto median_ns_per_call(Fn fn, std::size_t calls, int runs) -> double {
    std::vector<double> samples;
    samples.reserve(runs);

    for (int run = 0; run < runs; ++run) {
        auto start = std::chrono::steady_clock::now();
        auto result = fn();
        auto stop = std::chrono::steady_clock::now();
        keep_alive(result);
        samples.push_back(
            std::chrono::duration<double, std::nano>(stop - start).count() /
            double(calls));
    }

    std::sort(samples.begin(), samples.end());

    return samples[samples.size() / 2];
}

// How many slots the v-table table ended up with: the memory cost that the
// saved instructions have to pay for.
template<class Registry>
auto table_slots() -> std::size_t {
    using type_hash =
        typename Registry::template policy<bom::policies::type_hash>;

    if constexpr (std::is_same_v<type_hash, void>) {
        return Registry::template state<
                   boost::openmethod::research::vptr_direct_vector>()
            .vptrs.size();
    } else {
        return type_hash::hash_range().second + 1;
    }
}

} // namespace

int main() {
    bom::initialize();

    std::cerr << RESEARCH_POLICY_NAME << ": "
              << table_slots<research_registry>() << " slots, "
              << 8 * table_slots<research_registry>() << " bytes\n";

    constexpr std::size_t laps = 200;
    constexpr std::size_t calls = laps * num_objects;
    constexpr int runs = 15;

    std::cout << "variant,mode,classes,ns_per_call\n";

    for (std::size_t classes : {std::size_t(8), std::size_t(24), num_classes}) {
        auto objects = make_objects(classes, 20260906);

        auto latency = median_ns_per_call(
            [&] {
                std::size_t index = 0;

                for (std::size_t i = 0; i < calls; ++i) {
                    index = visit(*objects[index]);
                }

                return index;
            },
            calls, runs);

        auto throughput = median_ns_per_call(
            [&] {
                std::size_t sum = 0;

                for (std::size_t lap = 0; lap < laps; ++lap) {
                    for (const auto& object : objects) {
                        sum += visit(*object);
                    }
                }

                return sum;
            },
            calls, runs);

        std::cout << RESEARCH_POLICY_NAME << ",latency," << classes << ","
                  << latency << "\n"
                  << RESEARCH_POLICY_NAME << ",throughput," << classes << ","
                  << throughput << "\n";
    }

    return 0;
}
