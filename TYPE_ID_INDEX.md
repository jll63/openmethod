# Mapping a `type_info*` to an index by subtraction and division

Research note for [boostorg/openmethod#57](https://github.com/boostorg/openmethod/issues/57).
The prototypes and the harness are in `research/type_id_index/`; the sample
generator and the address analysis are in a separate repository,
`typeinfo-samples` (`make-libs.py`, `make-samples.sh`, `collect-samples.sh`,
`analyze-type-ids.py`).

**Verdict up front.** The mapping works, and works unconditionally - there is a
proof, not a search. The dispatch code it generates is exactly as good as
predicted: one fused load where `fast_perfect_hash` needs a multiply, a shift
and a load, and that is worth a real if modest 0.3-1.1 ns per call on two
compilers. It dies on memory: 12x-149x more than `fast_perfect_hash` even
in the single-module best case, and a multi-terabyte table the moment a program
`dlopen`s a module that registers classes of its own. (Implicitly linked shared
libraries are *not* a problem - copy relocations unify their type_info objects
into the executable; §5.) Recommendation: do not ship it as the general v-table
lookup.

The measurements did turn up something else, which is the more useful result of
the exercise: on realistic type id distributions `fast_perfect_hash`'s *search*
is far more expensive than its documentation suggests - 97 ms for 1000 classes
in one of the measured samples, and on the 4000-class sample it **gives up**
after 500 000 attempts and aborts. That is worth its own issue.

A second idea, tried afterwards (§11), fares much better: pick the *minimal
cover* of bit positions that still separates the type ids and pack them with
`pext`. It is immune to where the modules are - the `dlopen` case costs one
extra bit - costs 2x-32x `fast_perfect_hash`'s memory rather than 12x-infinity,
dispatches as fast, and finds its table deterministically in milliseconds on
the very input where `fast_perfect_hash` aborts. It is the one worth pursuing.

## 1. The idea

`type_id` is `const void*`; under `policies::std_rtti` it is `&typeid(X)`, the
address of an Itanium ABI `type_info` record. `policies::fast_perfect_hash`
turns it into a v-table table index with

```
H(x) = (mult * x) >> shift
```

searching at `initialize` time for a `mult` that is collision-free over the
registered type ids (`include/boost/openmethod/policies/fast_perfect_hash.hpp:36-45`,
`:196-286`). The proposal is to replace that with

```
H(x) = (x - base) / d
```

where `base` is the lowest registered type id.

## 2. What the ABI guarantees

Record sizes, confirmed with `readelf -W --dyn-syms` on this machine:

| C++ shape | ABI record | size |
|---|---|---|
| no base | `__class_type_info` | 16 |
| one public non-virtual base at offset 0 | `__si_class_type_info` | 24 |
| one virtual base | `__vmi_class_type_info` | 40 |
| *k* bases | `__vmi_class_type_info` | 24 + 16*k* |

Every record is therefore **at least 16 bytes**, and distinct objects do not
overlap, so two distinct type ids differ by at least 16. Since `a - b >= d`
implies `a/d > b/d`, the map is **injective for every `d <= 16`,
unconditionally**.

Three consequences:

- there is nothing to search for, and `initialize` is a min/max scan;
- the policy has no correctness failure mode - only a memory one;
- `d = 16` is always safe, which halves the table relative to `d = 8`; but only
  `d = 8 == sizeof(vptr_type)` allows the fused addressing described in §5.

`research/type_id_index/check_prototype.cpp` asserts the 16-byte separation and
the injectivity at both divisors, and is compiled and run for every variant
including the stock-policy control.

## 3. Method

Real libraries were measured only as a sanity anchor; the data set is synthetic,
built to vary the two things that drive the answer.

`make-libs.py` generates libraries with a controlled mix of the four class
shapes above (`--frac-root/-single/-virtual/-multi`) and a random number of
virtual functions per class (`--methods-per-class`). Two properties of the
generator turned out to matter more than the shape mix, and both were bugs in
the original version:

- **Key functions.** With every method inline, a class's v-table and `type_info`
  are COMDAT sections named `.data.rel.ro._ZTV…` / `._ZTI…`; the linker's name
  ordering then packs all the records together. Real classes have a key
  function, so the two land in ordinary `.data.rel.ro`.
- **Translation units.** Within one TU the compiler emits all the `type_info`
  objects together and all the v-tables after them, so a single-TU library packs
  its records back to back - **100 % occupancy**, an artefact. The generator now
  emits a header plus one TU per `--classes-per-tu` classes.

The pre-existing samples in `typeinfo-samples` had *both* problems plus no
inheritance at all, so their exact 16-byte stride was true by construction.
They must not be cited as evidence.

Runtime addresses come from `build/collect-type-infos` unchanged (it `dlopen`s
each combination in a forked process and reports what `dlsym` resolves, i.e.
what `&typeid(X)` yields), joined offline with `readelf`/`c++filt` for each
record's size and each module's load base. 20 ASLR replicates for the reference
configuration, 3 for the others.

Calibration: real libraries sit at 0.4 %-7.0 % slot density at `d = 8`
(libLLVM-20: 3607 classes, 6.8 MB span, 0.42 %; libstdc++: 292 classes, 33 KB,
7.04 %). The synthetic configurations bracket that, 0.33 %-10.7 %. The
conclusion does not depend on where in the band one lands.

## 4. What it costs

**One module** (1000 classes each). `fph table` is `fast_perfect_hash`'s bucket
count, 8 bytes per slot.

| config | span | slots@8 | density@8 | table@8 | fph table | blowup |
|---|---|---|---|---|---|---|
| si-heavy-tu1000 *(single-TU artefact)* | 34 KB | 4247 | 23.6 % | 33 KB | 16 KB | 2x |
| flat | 75 KB | 9326 | 10.7 % | 72 KB | 16 KB | 5x |
| tree | 195 KB | 24380 | 4.1 % | 190 KB | 16 KB | 12x |
| si-heavy | 808 KB | 100964 | 1.0 % | 788 KB | 16 KB | 49x |
| virt-heavy | 1.5 MB | 187190 | 0.5 % | 1.4 MB | 16 KB | 91x |
| mi-heavy | 2.4 MB | 305079 | 0.3 % | 2.3 MB | 16 KB | 149x |

**Four modules** (4000 classes): 55x (flat) to 497x (mi-heavy); the reference
configuration is 231x, 14.4 MB against 64 KB.

**Subsets make it worse, not better.** A program registers only some of the
classes its modules contain, and the unregistered records still sit between
them. Registering a uniformly random tenth of the reference sample leaves the
span unchanged and multiplies the blowup by ten: 2526x. Only a *contiguous*
window shrinks the span, and even then the density stays near 1 %.

## 5. Where it dies: dynamic loading, not shared libraries

The distinction matters, and an earlier draft of this note got it wrong. It is
*not* true that any program spanning an executable and a shared library has an
unusable span. Two cases behave completely differently.

**Implicitly linked DSOs are fine.** The Itanium ABI requires `type_info`
identity to be pointer identity across modules, and the linker delivers that
with a copy relocation: a type_info the executable names is copied into the
executable's image, and the DSO's references are redirected to that copy.
Measured directly (`g++ -O2 -fPIC -shared` + `-ldso`, default visibility):

```
exe    typeid(InExe)                    0x64263049ed78
exe    typeid(InLib) as seen by the exe 0x64263049ed50
lib    typeid(InLib) as seen by the lib 0x64263049ed50   <- same address
exe    typeid(std::runtime_error)       0x64263049ed60
```

40 bytes apart, all in the executable. `std::runtime_error` and
`std::bad_alloc` come along the same way. A program and its implicitly linked
libraries therefore have exactly the layout of §4 - the flat table is only as
bad as the single-module numbers.

**`dlopen` is fatal.** A plugin's own classes are not named by the executable,
so nothing unifies them; they stay in the plugin's own mapping, which the loader
places wherever it likes:

```
exe    InExe      0x60bc535edd90
plugin InPlugin   0x75e9405c4dc8
span = 23 282 698 776 632 bytes = 21.2 TB   (41.5 TB on the next run - ASLR)
```

That is 2.9e12 slots. The placement model in the table below is the same
scenario computed from the sample data, and lands in the same range:

| placement | span | slots@8 | table@8 | vs `fast_perfect_hash` |
|---|---|---|---|---|
| adjacent (several `dlopen`ed libraries, packed together) | 3.1 MB | 408 272 | 3.1 MB | 50x |
| observed (as ASLR actually placed those libraries) | 14.4 MB | 1 893 597 | 14.4 MB | 231x |
| split (executable + a `dlopen`ed module) | 31.6 TB | 4.3e12 | **31.6 TB** | 5e8x |

So the rule is sharper than "more than one module": the flat table survives
static and implicit dynamic linking, and dies on `dlopen`. Hidden visibility is
the other unifier-defeating configuration - openmethod's own shared-library
tests mark their classes `BOOST_SYMBOL_VISIBLE` precisely because RTTI must stay
default-visibility - and `augment_classes()` already has to cope with one class
carrying different type ids in different modules. Those extra ids widen the span
rather than break the mapping.

The prototype detects the condition and reports `too_sparse` rather than
attempting the allocation. It also fires well before the multi-module case: 1000
classes in *one* module spanning 807 704 bytes need 100 964 slots, 101 per
class, past the prototype's 64-per-class guard.

## 6. Dispatch: the codegen claim holds, and it is worth a little

Four variants were built (`RESEARCH_POLICY=fph|index|index16|direct`) and
disassembled with the `research-disasm` target. The sequence between loading the
`type_info` pointer and the indirect call, Intel syntax, GCC 13.3:

```asm
; fph - policies::fast_perfect_hash + vptr_vector
imul   rax, QWORD PTR [rip+...]     ; mult
shr    rax, cl                      ; shift, in cl
mov    rax, QWORD PTR [rdx+rax*8]

; index - research::direct_type_id_index<8> + vptr_vector
sub    rax, QWORD PTR [rip+...]     ; base
shr    rax, 0x3                     ; immediate shift
mov    rax, QWORD PTR [rdx+rax*8]

; direct - research::vptr_direct_vector
mov    rax, QWORD PTR [rdx+rax*1]   ; rdx = vptrs.data() - min_type_id
```

`direct` is exactly the single fused load the idea promises: the `type_id` *is*
the byte offset, because a slot is `sizeof(vptr_type)` bytes and the ids are at
least 16 bytes apart. clang 22 does better than the above on `index` too - it
scales the table so the `>>3` disappears into the addressing, leaving `sub` plus
the fused load - and reproduces the single `mov` for `direct`. Note that GCC
needs the `vptr` seam for that: `vptr_vector` ends in `vptrs[index]`, and GCC
does not prove that `type_id - base` is a multiple of 8.

Wall clock, ns per call, 4096 objects, 64 registered classes, calls through a
plain `const node_base&` (a `virtual_ptr` argument never looks a v-table up).
Median of 5 program runs, each itself the median of 15 timed loops; the +- is
the half-range across the 5 runs.

GCC 13.3:

| mode | classes | fph | index | index16 | direct |
|---|---|---|---|---|---|
| latency | 8 | 8.74+-0.40 | 8.58+-0.32 | 8.28+-0.28 | 8.46+-0.25 |
| latency | 24 | 8.61+-0.39 | 9.18+-0.39 | 8.52+-0.30 | 8.63+-0.20 |
| latency | 64 | 8.57+-0.19 | 8.94+-0.41 | 8.67+-0.34 | 8.79+-0.27 |
| throughput | 8 | 8.15+-0.35 | 7.58+-0.31 | 7.28+-0.25 | **6.94**+-0.28 |
| throughput | 24 | 8.39+-0.37 | 8.30+-0.34 | 7.73+-0.28 | **7.71**+-0.29 |
| throughput | 64 | 8.40+-0.26 | 7.85+-0.35 | 7.93+-0.31 | **7.33**+-0.31 |

clang 22:

| mode | classes | fph | index | direct |
|---|---|---|---|---|
| latency | 8 | 8.65+-0.08 | **8.29**+-0.29 | 8.49+-0.27 |
| latency | 24 | 9.00+-0.24 | **8.65**+-0.29 | 8.71+-0.28 |
| latency | 64 | 9.17+-0.13 | 8.83+-0.26 | **8.81**+-0.29 |
| throughput | 8 | 7.99+-0.22 | 7.54+-0.17 | **7.47**+-0.06 |
| throughput | 24 | 8.41+-0.30 | 8.07+-0.18 | **7.97**+-0.08 |
| throughput | 64 | 8.63+-0.22 | 8.12+-0.07 | **7.98**+-0.08 |

The two compilers agree on the direction at every point: `direct` beats `fph` by
0.6-1.1 ns per call on throughput, and clang shows 0.2-0.4 ns on latency as
well, where GCC shows nothing. At ~4 GHz that is roughly 1.5-4.5 cycles - the
right order for removing a dependent `imul` and a shift. It is not a cache
effect: the tables here are 1.0 KB (fph), 1.5 KB (index, direct) and 0.8 KB
(index16).

*Correction.* An earlier draft of this note reported no measurable difference.
That measurement used a 512-class benchmark whose larger tables and heavier
object mix buried the effect - and whose compilation reached 23.5 GB resident
per translation unit, enough to fire the OOM killer. The benchmark now
registers 64 classes and compiles in 0.7 GB; see §10.

## 7. Initialize: the incumbent is the problem

Timing `type_hash::initialize` + `vptr::initialize` against a stand-in
`InitializeContext`, so the policies are measured in isolation.

On the **measured** address sets (1000 classes, one module), search time alone:

| sample | `fast_perfect_hash` | `direct_type_id_index` |
|---|---|---|
| flat | 0.20 ms | ~1 µs |
| tree | 21.7 ms | ~1 µs |
| si-heavy | **96.9 ms** | ~1 µs |

On synthetic ids at a controlled sparsity (`stride` = mean bytes of span per id):

| stride | n | fph total | index total | ratio |
|---|---|---|---|---|
| 16 | 65536 | 456 µs | 81 µs | 5.6x |
| 64 | 1024 | 31.3 ms | 1.2 µs | 25 000x |
| 64 | 65536 | 2.91 s | 89 µs | 33 000x |
| 256 | 1024 | 91.6 ms | 1.2 µs | 74 000x |
| 256 | 65536 | **43.7 s** | 110 µs | 397 000x |

`fast_perfect_hash` is fast when the ids are evenly spaced and degrades
catastrophically when they are not - which is what real layouts look like. The
cost is structural: up to 5 passes x 100 000 attempts, each attempt refilling a
`2^M` bucket array and re-inserting every id
(`fast_perfect_hash.hpp:213-277`). `test/test_slot_allocator.cpp:327-337`
already notes this in passing; these are numbers for it.

## 8. Refinements

- **R0 - rebasing.** Not an optimisation but a requirement: `vptr_vector` sizes
  its vector from `hash_range().second + 1`, so `hash` must return a 0-based
  index. An un-rebased `type_id / 8` would ask for ~2^44 entries. Conversely the
  `[0, min_value)` prefix that `vptr_vector` wastes for `fast_perfect_hash` is
  worth nothing there - with 1.25n-2.5n buckets, `min_value` is 1 or 2.
- **R1 - `d = 16`.** Always injective, halves the table, and measures the same as
  `d = 8` (`index16` above). But it forfeits the fused load, since a slot is 8
  bytes. Worth it only if the flat table were viable, which it is not.
- **R2 - hybrid.** A runtime fallback needs a branch on the hot path and gives
  up the fused addressing; the honest form is a compile-time choice plus an
  `initialize`-time diagnostic, which is what `too_sparse` is.
- **R3 - page directory.** `vptr = dir[(ti>>12) & mask][(ti & 0xff8)>>3]`
  removes the multi-module span problem entirely, since only occupied pages are
  allocated. Measured on the same data: 305 KB-1.7 MB against 64 KB for
  `fast_perfect_hash`, at the cost of two dependent loads. Cheaper than the flat
  table by two orders of magnitude, still 5x-26x worse than the incumbent and
  slower to boot. Not promising.
- **R4 - per-module base.** Rejected on inspection: recovering the module from a
  pointer needs a range search, several instructions and a mispredictable branch.
  R3 is the same idea done with a mask.

## 9. Go/no-go

The criteria, fixed before the measurements:

| | criterion | outcome |
|---|---|---|
| G1 | whitelisted tests pass, with and without runtime checks | **pass** - 80/80 (`ctest -R '^research-'`), 4 variants x 2 check modes |
| G2 | reference path >= 1.5 cycles/dispatch faster, on >= 2 compilers | **pass, narrowly** - 0.6-1.1 ns (~2.5-4.5 cycles) on throughput, GCC and clang agreeing; latency moves only on clang |
| G3 | table <= 8x `fast_perfect_hash` on realistic subsets | **fail** - 12x-149x in one module, 231x in four |
| G4 | multi-module case diagnosed, with a checkable rule | **pass** - `too_sparse`, and the rule is precise: no `dlopen`ed module may register classes |

**Do not promote this to a policy** - but the reason is now G3 alone, and it is
narrower than it first looked. The speed argument holds, and the multi-module
objection turns out to apply only to `dlopen`, not to shared libraries in
general. What remains is that the table costs 12x-149x `fast_perfect_hash`'s
even in the single-module best case, to buy a few percent on the one dispatch
path that uses the map. The page-directory variant that would survive the
`dlopen` case is worse than the incumbent on both memory and latency.

If it were ever revisited, the shape to revisit is a *compile-time* opt-in for
programs that do not `dlopen` class-registering modules - the memory is the
user's to spend, and `too_sparse` already diagnoses the case where it cannot be.
That is a bigger change to the policy surface than the few percent justifies.

Two things are worth carrying forward:

1. **`fast_perfect_hash`'s search deserves its own issue.** 97 ms for 1000
   classes on measured addresses, and seconds at 10^4-10^5, is a real cost, and
   the failure mode is a `search_error` + `abort`. The bounded, proof-backed
   min/max scan measured here would be a sound basis for a *fallback* or for a
   better-seeded search, even though it is useless as the primary map.
2. **The injectivity theorem is reusable.** Any future scheme keyed on
   `std::type_info` addresses can rely on a guaranteed 16-byte separation.

## 10. A note on building the harness

`bench_dispatch.cpp` registers a forest of `node<I> : node<(I-1)/2>` classes,
one overrider each. The compile cost is roughly quadratic in the class count -
`use_classes` relates every pair - and at 512 classes one translation unit
reached **23.5 GB resident and 2m37s** on GCC 13.3. Built `-j12`, that is enough
to exhaust the machine and fire the *global* OOM killer, which then picks off
whatever is largest, editor and shell included. At 64 classes the same TU takes
0.7 GB and 5 s, which is why the benchmark registers 64.

Build it under a memory cap regardless:

```bash
capped -m 24G -- cmake --build build-research --target research-all -j4
```

## 11. Second idea: a minimal-cover `pext` index

The flat table fails because the *span* sets the table size. But a set of
addresses clustered per module and far apart between modules has structure in
its bits: the low bits vary within a module, the high bits vary between modules
- and the high bits are perfectly correlated with one another, since they all
encode nothing but "which module". Only about `log2(modules)` of them carry
information.

So find the **minimal cover**: the smallest set of bit positions found such
that `x & mask` is still injective over the registered type ids, and index with
`pext(x, mask)` - BMI2 parallel bit extract, one instruction, which packs the
selected bits into `[0, 2^popcount(mask))`. Injectivity of `pext(x, mask)` is
exactly injectivity of `x & mask`, so the search never needs `pext`. After
"Perfect Hashing in an Imperfect World", Joaquín M. López Muñoz.

The search is deterministic. The bits that vary at all are trivially a cover;
one greedy pass drops bits lowest-entropy-first while injectivity holds; a
second builds a cover bottom-up by adding the bit that resolves the most
collisions, then trims it the same way; the smaller wins.

### 11.1 On the data

`analyze-bits.py`, same samples as §4-5 (`bits.md` has every configuration):

| sample | modules | N | varying bits | **cover** | floor | table | fph table | blowup | *flat table (§4-5)* |
|---|---|---|---|---|---|---|---|---|---|
| flat, 1 module | 1 | 1000 | 14 | **12** | 10 | 32 KB | 16 KB | 2x | 5x |
| tree, 1 module | 1 | 1000 | 15 | **13** | 10 | 64 KB | 16 KB | 4x | 12x |
| si-heavy, 1 module | 1 | 1000 | 18 | **14** | 10 | 128 KB | 16 KB | 8x | 49x |
| mi-heavy, 1 module | 1 | 1000 | 21 | **15** | 10 | 256 KB | 16 KB | 16x | 149x |
| si-heavy, 4 modules, observed | 4 | 4000 | 22 | **18** | 12 | 2 MB | 64 KB | 32x | 231x |
| si-heavy, 4 modules, adjacent | 4 | 4000 | 19 | **17** | 12 | 1 MB | 64 KB | 16x | 50x |
| **si-heavy, exe + `dlopen`ed** | 4 | 4000 | **31** | **18** | 12 | 2 MB | 64 KB | 32x | **5e8x (31 TB)** |
| flat, exe + `dlopen`ed | 4 | 4000 | 30 | **14** | 12 | 128 KB | 64 KB | 2x | - |
| tree, exe + `dlopen`ed | 4 | 4000 | 31 | **16** | 12 | 512 KB | 64 KB | 8x | - |

The `dlopen` catastrophe is gone: 31 varying bits cover in 18, the same as the
observed placement. The 13 high bits that differ between the executable and the
plugin are one piece of information, and the search keeps one of them. What
remains is the within-module dilution, 2-5 bits above the `ceil(log2 N)` floor:
the price of choosing a bit *subset* rather than a 64-bit multiplier.

### 11.2 Prototype C - `research::pext_index<MaxBits>`

A `type_hash` policy, same shape as prototype A; state is `{mask, max_value}`
plus the control table under runtime checks. `hash` is `_pext_u64(ti, mask)`.
Needs `-mbmi2`; the CMake harness adds it to the `pext` variant only, so the
other variants measure what a stock build would. 20/20 of the re-run tests
pass, checked and unchecked.

Codegen, GCC 13.3, between the `type_info` load and the indirect call:

```asm
mov    rdx, QWORD PTR [rip+...]           ; table
pext   rax, rax, QWORD PTR [rip+...]      ; mask, as a memory operand
mov    rax, QWORD PTR [rdx+rax*8]
```

One `pext` (3-cycle latency on Zen 3+ and Intel; **microcoded, ~18 cycles, on
Zen 1/2**) where `fast_perfect_hash` has `imul` + `shr`.

### 11.3 Dispatch

Same protocol as §6, ns per call, 64 registered classes, GCC 13.3, Zen 5:

| mode | classes | fph | pext | direct |
|---|---|---|---|---|
| latency | 8 | 8.55+-0.28 | 8.63+-0.27 | 8.47+-0.26 |
| latency | 24 | 8.98+-0.36 | 8.81+-0.27 | 8.82+-0.30 |
| latency | 64 | 9.03+-0.29 | 8.93+-0.30 | 8.90+-0.28 |
| throughput | 8 | 8.02+-0.31 | 7.93+-0.25 | 7.40+-0.27 |
| throughput | 24 | 8.81+-0.33 | 7.84+-0.22 | 7.82+-0.32 |
| throughput | 64 | 8.76+-0.28 | 8.40+-0.39 | 7.94+-0.09 |

Same speed class as `fast_perfect_hash`, perhaps a hair better on throughput;
`direct` keeps its fused-load edge. Table on this benchmark: 126 slots against
`fast_perfect_hash`'s 128 - for tightly packed classes the cover is as good as
the hash. This is not a speed play.

### 11.4 Initialize - where it wins

Same protocol as §7. On the **measured** address sets:

| sample | `fast_perfect_hash` | `pext_index` | flat table |
|---|---|---|---|
| flat, 1000 | 0.20 ms | 1.31 ms | ~1 µs |
| tree, 1000 | 21.7 ms | 1.39 ms | ~1 µs |
| si-heavy, 1000 | 96.9 ms | 2.00 ms | *aborts (`too_sparse`)* |
| mi-heavy, 1000 | 96.9 ms | 2.43 ms | *aborts* |
| si-heavy, 4 modules, 4000 | **aborts** | 12.9 ms | *aborts* |
| mi-heavy, 4 modules, 4000 | - | 15.7 ms | *aborts* |

`fast_perfect_hash` on the 4000-class set: *"could not find hash factors after
500000 attempts using up to 262144 buckets"*, then `abort()`. That is 5 passes
x 100 000 multipliers, the table grown to 2^18 slots - 2 MB, the same size as
`pext_index`'s 18-bit cover - and still no perfect hash. On the same input
`pext_index` finds its cover, deterministically, in 12.9 ms.

The cover search is `O(bits x N log N)` with a sort per candidate; the
bottom-up pass dominates. Synthetic sweep, total initialize: 1024 ids
0.8-2.5 ms, 65 536 ids 142-229 ms, regardless of stride - between the flat
scan's ~100 µs and `fast_perfect_hash`'s 0.5 ms (regular ids) to 44 s (sparse
ids). A hash set instead of a sort, and a cheaper add pass, would take a good
factor off; nothing here is tuned.

### 11.5 Assessment

Against the §9 criteria: G1 passes; G2 is neutral - as fast as the incumbent,
not faster; G3 is 2x-32x, so it clears the 8x bar in the single-module
configurations and misses it across modules; G4 passes with *no* restriction -
`dlopen` costs one bit. Two caveats are real:

- **BMI2.** Absent on ARM and on x86 before Haswell/Zen; slow on Zen 1/2. A
  software `pext` is a loop over the mask bits, far too slow for the hot path;
  the honest fallback is `fast_perfect_hash` itself, chosen at compile time.
- **Memory is still above the incumbent** wherever `fast_perfect_hash`'s search
  succeeds - and it usually does, up to a few hundred classes per module.

What it delivers that nothing else in this note does: a table that is found
**deterministically, in bounded time, on inputs where the incumbent aborts**,
with no dependence on module placement. That makes it the natural *fallback*
for `fast_perfect_hash`'s `search_error` path, and a candidate primary on
BMI2 hardware for programs with large registries. Either way the memory numbers
above are the ones to weigh, not the flat table's.

## Reproducing

```bash
# data (separate repo)
cd ~/dev/typeinfo-samples
./make-samples.sh && ./collect-samples.sh
python3 analyze-type-ids.py --export-type-ids exported   # -> type-ids.csv, type-ids.md

# prototypes and measurements
cmake -B build-research -DBOOST_SRC_DIR=/path/to/boost \
      -DBOOST_OPENMETHOD_BUILD_TESTS=ON -DBOOST_OPENMETHOD_BUILD_RESEARCH=ON \
      -DCMAKE_BUILD_TYPE=Release
capped -m 24G -- cmake --build build-research --target research-all -j4  # see 10
ctest --test-dir build-research -R '^research-'          # correctness
cmake --build build-research --target research-disasm    # codegen
build-research/research/type_id_index/research-{fph,index,index16,direct,pext}-bench_dispatch
build-research/research/type_id_index/research-{fph,index,pext}-bench_initialize
cd ~/dev/typeinfo-samples && python3 analyze-bits.py     # -> bits.md (§11)
```

The harness is off by default (`BOOST_OPENMETHOD_BUILD_RESEARCH`), has no
Jamfile, and is not part of the library's API.
