# Mapping a `type_info*` to an index by subtraction and division

Research note for [boostorg/openmethod#57](https://github.com/boostorg/openmethod/issues/57).
The prototypes and the harness are in `research/type_id_index/`; the sample
generator and the address analysis are in a separate repository,
`typeinfo-samples` (`make-libs.py`, `make-samples.sh`, `collect-samples.sh`,
`analyze-type-ids.py`).

## The five schemes

#57 asked about one mapping; four more were tried against it. Each is named by
a letter throughout, and every measurement table is keyed on the harness
variant that builds it (`-DRESEARCH_POLICY=`, see `registry.hpp`):

| | scheme | § | variant | header | the map |
|---|---|---|---|---|---|
| **A** | flat index | §1-9 | `index`, `index16` | `direct_type_id_index.hpp` | `(x - base) / d` |
| **B** | flat index, fused load | §1-9 | `direct` | `vptr_direct_vector.hpp` | `x` *is* the byte offset |
| **C** | minimal-cover `pext` | §10 | `pext` | `pext_index.hpp` | `pext(x, mask)` |
| **D** | hash and displace | §11 | `hd`, `hdmin` | `hd_index.hpp` | `mulhi(h * pilots[h >> k], slots)` |
| **E** | two multiply-shifts | §13 | `twolevel`, `twolevel1` | `two_level_index.hpp` | `(m2[h >> s1] * h) >> s2` |
| - | per-module directory | §12 | *not built* | - | `dir[hash(x >> 12)] + (x >> 3)` |

`fph` is the control throughout: stock `default_registry`, whose
`policies::fast_perfect_hash` searches for a `mult` such that
`H(x) = (mult * x) >> shift` is collision-free. Against it, and against each
other:

| | table | initialize | dispatch | survives `dlopen` |
|---|---|---|---|---|
| **A**, **B** - flat index (§1-9) | 5x-5e8x | fastest | **-0.6 to -1.1 ns** | no |
| **C** - `pext` cover (§10) | 2x-32x, or 1x-2x measured | 20x-40x faster | neutral | yes |
| **D** - hash and displace (§11) | **0.07x-0.58x** | **3x-5000x faster** | +1.3 to +2.5 ns | yes |
| per-module directory (§12) | 4.5x-138x | not built | not built | yes |
| **E** - two multiply-shifts (§13) | 0.07x-1.1x | as D | +0.4 to +2.4 ns | yes |

Table sizes are ratios to `fast_perfect_hash`'s, dispatch is ns per call
against it (negative is faster); §4 explains which `fast_perfect_hash` baseline
the table column is measured against, and why that choice overstates every
ratio in it.

## Verdict

**None of the five is worth shipping as the general v-table lookup.**
`fast_perfect_hash` is a multiply and a shift, and nothing here gets under it:
A and B buy about a nanosecond with unusable amounts of memory, D buys the
memory back and gives up two, C sits between them and is neutral on both, E is
D with a nanosecond of its cost recovered on one compiler. What the exercise
does produce is a replacement for `fast_perfect_hash`'s *failure* path, and one
result that is not about a map at all (§15).

**A and B - the flat index.** The mapping works, and works unconditionally -
there is a proof, not a search (§2). The dispatch code it generates is exactly
as good as predicted: one fused load where `fast_perfect_hash` needs a
multiply, a shift and a load, and that is worth a real if modest 0.3-1.1 ns per
call on two compilers. It dies on memory: 5x-149x more than `fast_perfect_hash`
even in the single-module best case, and a multi-terabyte table the moment a
program `dlopen`s a module that registers classes of its own. (Implicitly
linked shared libraries are *not* a problem - copy relocations unify their
type_info objects into the executable; §5.)

**C - the minimal-cover `pext` index.** The flat table fails because the
*span* sets its size; pick instead the smallest set of bit positions that still
separates the type ids, and pack them with `pext`. C is immune to where the
modules are - the `dlopen` case costs one extra bit - dispatches as fast as the
incumbent, and finds its table deterministically in milliseconds on the very
input where `fast_perfect_hash` aborts. On memory it is 2x-32x the incumbent
against the *modelled* baseline, and near parity against the *measured* one;
§4 says why those two differ and §14.1 has the measurements.

**D - a minimal perfect hash, by hash and displace.** The other extreme: one
slot per type id, whatever the addresses are. It is the only scheme here that
costs **less** memory than `fast_perfect_hash` - a flat 9.45 bytes per type id
against 16 KB-128 KB for a thousand classes - and it builds that table
3x-5000x faster, on every input including the ones that make the incumbent
abort. It pays with a second dependent load on the dispatch path, and it is
measurably slower for it: +1.3 to +2.5 ns per call on both compilers.

**The per-module directory**, suggested after the others, hashes the per-module
high bits into a directory of per-module coefficients. It is the honest form of
§8's R4 and it works: the `dlopen` span disappears for a directory of under
2 KB. But the table that remains is the sum of the per-module spans, which is
what §4 measured, so it inherits that in full and is dominated on memory by
both C and D, at a dispatch sequence no shorter than either. It was analysed,
not built.

**E - two multiply-shifts** is D with the final reduction replaced by a shift
into a power-of-two table. It recovers about a nanosecond of D's cost on GCC
and none of it on clang, and it trades D's flat table size for a sawtooth
between 1.0 and 2.0 slots per type id.

**The by-product is the more useful result of the exercise.** On realistic type
id distributions `fast_perfect_hash`'s *search* is far more expensive than its
documentation suggests - up to 175 ms for 1000 classes on the measured
samples,
and on the 4000-class sample it **gives up** after 500 000 attempts and aborts.
That is worth its own issue. C and D are both candidates for the path where it
gives up, and D is the stronger of the two (§10.5, §11.6).

**MSVC.** The whole harness was rebuilt and re-run with MSVC 19.51 (§14). Every
prototype's table comes out the same integer there as on GCC, and
`fast_perfect_hash`'s does not - its randomized search lands on different
factors under a different toolchain. MSVC agrees on the one result that
matters, that D costs measurably more per call, and finds the rest inside its
noise. It also turned up the largest per-call effect measured anywhere in this
note, and not in a map at all: MSVC does not inline fetching an object's type
id, and doing it by hand saves about half a nanosecond per dispatch - twice the
biggest difference between any two maps there (§15).


## 1. Prototypes A and B: the idea

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

That formula is **prototype A**, `research::direct_type_id_index<d>`, a
`type_hash` policy like the incumbent, built as the `index` (`d = 8`) and
`index16` (`d = 16`) variants. **Prototype B**,
`research::vptr_direct_vector`, is the same idea taken one step further: at
`d = 8` a slot is exactly `sizeof(vptr_type)` bytes, so the type id *is* the
byte offset into the v-table table, and folding `base` into that table's
address leaves a single fused load with no arithmetic in front of it (§6). B
is a `vptr` policy and replaces `vptr_vector`, so it needs no `type_hash`
policy at all; it is the `direct` variant. The two share every property
established in §§2-5 - they differ only in where the subtraction happens.

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

**The `fast_perfect_hash` baseline is modelled here, and the model is
optimistic.** `analyze-type-ids.py`'s `fph_buckets(n)` returns the bucket count
the incumbent *starts from* - `1.25n` rounded up to a power of two, so 2048
slots, 16 KB, for a thousand classes. The search grows the table when it
cannot place the ids in that many buckets, and on these samples it usually
does: §14.1 measured what it actually allocates, and only `flat` stops at 2048.

| sample | modelled | measured (§14.1) |
|---|---|---|
| flat, 1000 | 16 KB | 16 KB |
| tree, 1000 | 16 KB | **32 KB** |
| si-heavy, 1000 | 16 KB | **128 KB** |
| mi-heavy, 1000 | 16 KB | **128 KB** |

The modelled figure is the one used in this section and in §10.1, because it is
the only one available for the 4000-class samples - there the search aborts and
allocates nothing at all. Every ratio computed against it is therefore an
*upper* bound on the true blowup, by up to 8x. §11.3 and §14.1 use the measured
figure instead and say so. Where the two baselines change a conclusion rather
than a decimal - which is only in §10.5 - both are given.

**One module** (1000 classes each), against the modelled baseline:

| config | span | slots@8 | density@8 | table@8 | fph table (modelled) | blowup |
|---|---|---|---|---|---|---|
| si-heavy-tu1000 *(single-TU artefact)* | 34 KB | 4247 | 23.6 % | 33 KB | 16 KB | 2x |
| flat | 75 KB | 9326 | 10.7 % | 72 KB | 16 KB | 5x |
| tree | 195 KB | 24380 | 4.1 % | 190 KB | 16 KB | 12x |
| si-heavy | 808 KB | 100964 | 1.0 % | 788 KB | 16 KB | 49x |
| virt-heavy | 1.5 MB | 187190 | 0.5 % | 1.4 MB | 16 KB | 91x |
| mi-heavy | 2.4 MB | 305079 | 0.3 % | 2.3 MB | 16 KB | 149x |

Discounting the single-TU artefact, that is **5x-149x**, and this note quotes
that range throughout. Against the measured baseline the same rows are 5x
(`flat`), 6x (`tree`) and 6x (`si-heavy`); `mi-heavy` is 18x.

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
registers 64 classes and compiles in 0.7 GB; see the appendix.

## 7. Initialize: the incumbent is the problem

Timing `type_hash::initialize` + `vptr::initialize` against a stand-in
`InitializeContext`, so the policies are measured in isolation.

On the **measured** address sets (1000 classes, one module), search time alone:

| sample | `fast_perfect_hash` | `direct_type_id_index` |
|---|---|---|
| flat | 0.20 ms | ~1 µs |
| tree | 21.7 ms | ~1 µs |
| si-heavy | **96.9 ms** | ~1 µs |

**One figure per sample understates the variation, and the variation is in the
input, not the measurement.** `fast_perfect_hash`'s cost is a deterministic
function of the address set - seven invocations on one sample agree to 1 % - but
it swings by up to 2.5x across the four ASLR replicates of the *same*
configuration, because a different placement is a different search problem.
Re-measured across all four, `type_hash::initialize` alone, min to max in ms:

| sample | `fast_perfect_hash` | `pext_index` | `hd` | `hdmin` | fph spread |
|---|---|---|---|---|---|
| flat, 1000 | 0.198-0.203 | 1.23-1.48 | 0.062-0.098 | 0.025-0.031 | 1.0x |
| tree, 1000 | 21.5-54.0 | 1.57-1.68 | 0.049-0.088 | 0.024-0.034 | 2.5x |
| si-heavy, 1000 | 43.4-103.0 | 2.18-2.71 | 0.045-0.065 | 0.024-0.031 | 2.4x |
| mi-heavy, 1000 | 110.1-174.7 | 2.65-3.27 | 0.054-0.077 | 0.025-0.037 | 1.6x |

Two things follow. The single figures quoted in this section, §10.4 and §11.4
are each one replicate, so treat them as order-of-magnitude and compare columns
within a table rather than a column across tables. And **`mi-heavy` is the worst
sample, not a tie with `si-heavy`**: both of those tables give it 96.9 ms, below
every replicate measured here. The ordering the re-run establishes - `flat`
under a millisecond, `tree` tens, `si-heavy` tens to a hundred, `mi-heavy` over
a hundred - tracks the sparsity ordering of §4, which is the result the section
is making; the corrected `mi-heavy` figures strengthen it.

The prototypes' own columns vary by a few percent across replicates and by
10-15 % against the figures in §10.4 and §11.4, which were taken in an earlier
session. Their slot counts are identical to the last integer, in this re-run as
in §14.1. The 4000-class samples were re-run too: `fast_perfect_hash` aborts on
all four with the message quoted in §10.4, and `pext_index`, `hd` and `hdmin`
reproduce §14.1's slot counts exactly - 57 343 and 524 218 under the observed
placement, 260 970 and 261 865 under `pie-split`, against 4211 and 4000.

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
- **R4 - per-module base.** Rejected here on inspection, for the wrong reason -
  recovering the module needs no range search, only a mult-shift over the high
  bits. Measured properly in §12, where it removes the multi-module span
  entirely for a directory of under 2 KB, and is still dominated on memory by
  §10 and §11, at a dispatch sequence no shorter than either, because it
  leaves the within-module dilution of §4 untouched.

## 9. Go/no-go

The criteria, fixed before the measurements. They were written for A and B, and
the outcomes below are theirs; §§10.5, 11.6 and 13.6 score C, D and E against
the same four, so the wording is deliberately scheme-neutral:

| | criterion | outcome |
|---|---|---|
| G1 | whitelisted tests pass, with and without runtime checks | **pass** - 80/80 (`ctest -R '^research-'`), 4 variants x 2 check modes |
| G2 | reference path >= 1.5 cycles/dispatch faster, on >= 2 compilers | **pass, narrowly** - 0.6-1.1 ns (~2.5-4.5 cycles) on throughput, GCC and clang agreeing; latency moves only on clang |
| G3 | table <= 8x `fast_perfect_hash` on realistic subsets | **fail** - 5x-149x in one module, 231x in four |
| G4 | multi-module case diagnosed, with a checkable rule | **pass** - `too_sparse`, and the rule is precise: no `dlopen`ed module may register classes |

**Do not promote A or B to a policy** - but the reason is now G3 alone, and
it is narrower than it first looked. The speed argument holds, and the
multi-module objection turns out to apply only to `dlopen`, not to shared
libraries in general. What remains is that the table costs 5x-149x what
`fast_perfect_hash` needs even in the single-module best case, to buy a few
percent on the one dispatch path that uses the map. The page-directory variant
that would survive the `dlopen` case is worse than the incumbent on both memory
and latency.

If it were ever revisited, the shape to revisit is a *compile-time* opt-in for
programs that do not `dlopen` class-registering modules - the memory is the
user's to spend, and `too_sparse` already diagnoses the case where it cannot be.
That is a bigger change to the policy surface than the few percent justifies.

Two things are worth carrying forward:

1. **`fast_perfect_hash`'s search deserves its own issue.** 43-175 ms for 1000
   classes on measured addresses, and seconds at 10^4-10^5, is a real cost, and
   the failure mode is a `search_error` + `abort`. The bounded, proof-backed
   min/max scan measured here would be a sound basis for a *fallback* or for a
   better-seeded search, even though it is useless as the primary map.
2. **The injectivity theorem is reusable.** Any future scheme keyed on
   `std::type_info` addresses can rely on a guaranteed 16-byte separation.

## 10. Prototype C: a minimal-cover `pext` index

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

### 10.1 On the data

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

### 10.2 The policy - `research::pext_index<MaxBits>`

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

### 10.3 Dispatch

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

### 10.4 Initialize - where it wins

Same protocol as §7, whose note on comparing `fast_perfect_hash` figures across
tables applies here. On the **measured** address sets:

| sample | `fast_perfect_hash` | `pext_index` | flat table |
|---|---|---|---|
| flat, 1000 | 0.20 ms | 1.31 ms | ~1 µs |
| tree, 1000 | 21.7 ms | 1.39 ms | ~1 µs |
| si-heavy, 1000 | 96.9 ms | 2.00 ms | *aborts (`too_sparse`)* |
| mi-heavy, 1000 | **110-175 ms** (§7) | 2.43 ms | *aborts* |
| si-heavy, 4 modules, 4000 | **aborts** | 12.9 ms | *aborts* |
| mi-heavy, 4 modules, 4000 | **aborts** | 15.7 ms | *aborts* |

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

### 10.5 Assessment

Against the §9 criteria: G1 passes; G2 is neutral - as fast as the incumbent,
not faster; G4 passes with *no* restriction - `dlopen` costs one bit.

**G3 is the one criterion whose answer depends on which baseline §4 is read
with**, and it is worth being explicit, because the two readings say different
things:

| sample | cover | fph, modelled | fph, measured (§14.1) |
|---|---|---|---|
| flat, 1000 | 32 KB | 16 KB - 2x | 16 KB - **2x** |
| tree, 1000 | 64 KB | 16 KB - 4x | 32 KB - **2x** |
| si-heavy, 1000 | 128 KB | 16 KB - 8x | 128 KB - **1x** |
| mi-heavy, 1000 | 256 KB | 16 KB - 16x | 128 KB - **2x** |

Against the modelled baseline the cover costs 2x-32x, clearing the 8x bar in
the single-module configurations and missing it across modules. Against the
measured one it is **at or near parity on every sample where the incumbent
finishes at all** - 1x-2x - and the reason is not subtle: both schemes are
paying for the same thing. `fast_perfect_hash` grows its table when the ids are
sparse, and the `pext` cover widens by the same bits for the same reason. They
degrade together.

So G3 is a pass on the measured baseline and a partial pass on the modelled
one. The honest summary is that C costs about twice `fast_perfect_hash` on the
samples where `fast_perfect_hash` works, not the eight to thirty-two times the
modelled column suggests. One caveat survives either reading:

- **BMI2.** Absent on ARM and on x86 before Haswell/Zen; slow on Zen 1/2. A
  software `pext` is a loop over the mask bits, far too slow for the hot path;
  the honest fallback is `fast_perfect_hash` itself, chosen at compile time.

What it delivers that nothing else in this note does: a table that is found
**deterministically, in bounded time, on inputs where the incumbent aborts**,
with no dependence on module placement. That makes it the natural *fallback*
for `fast_perfect_hash`'s `search_error` path, and a candidate primary on
BMI2 hardware for programs with large registries. Either way the memory numbers
above are the ones to weigh, not the flat table's - and D (§11) beats it on
every one of them.

## 11. Prototype D: a minimal perfect hash, by hash and displace

A, B and C all let the *addresses* set the table size: the flat index pays for
the span, the `pext` cover pays for the within-module dilution. A
minimal perfect hash pays for neither. It maps `N` type ids onto `N` slots by
construction, so the table is `8N` bytes whatever the addresses are, and there
is no distribution that makes it larger.

The classical construction is "hash, displace and compress" (Belazzougui,
Botelho and Dietzfelbinger). Split the keys into `N / Lambda` buckets by one
hash; place the buckets largest first; give each bucket a *pilot*, found by
trial, that moves all of its keys onto slots that are still free. Lookup reads
the key's bucket, loads that bucket's pilot, and applies it.

The compression step is dropped here, and that is deliberate. Compressing the
pilots is what takes CHD to ~2 bits per key, but decoding a compressed integer
sequence on the dispatch path would cost far more than the bytes it saves. A
pilot is stored raw, so reading one is a single indexed load.

**Which is why the policy is `hd_index` and the variants are `hd` and `hdmin`,
not `chd`.** The published algorithm is *compress, hash and displace*, and its C
is the step this drops; carrying the letter would promise the ~2 bits per key
that the compression buys and this does not, against the 9.45 bytes §11.3
measures - a factor of 38. The construction is still CHD's, and it is cited as
such above; only the two steps that are implemented are named.

### 11.1 The formula, and one that does not work

`research::hd_index<Lambda, LoadPercent>`, a `type_hash` policy:

```cpp
h     = x * seed;                       // one multiply
pilot = pilots[h >> bucket_shift];      // top bits pick the bucket
index = mulhi64(h * pilot, slots);      // displace, then reduce
```

`slots` is `N * 100 / LoadPercent`, and `mulhi64` is Lemire's multiply-shift
reduction - the top half of a 64x64 product - which lands on `[0, slots)` for
any `slots`, with no division.

The displacement has to be **multiplicative**, and the first attempt got this
wrong in a way worth recording, because it fails structurally rather than
statistically. Adding a pilot, or exclusive-oring one in, leaves two keys in
the same relation to each other in their high bits, and the reduction reads
exactly those high bits: two ids that agree there collide under *every* pilot,
and no seed rescues the bucket. There are about `Lambda / 2` such pairs in a
table of `N` slots, so the search does not slow down, it stops - on the
synthetic sweep it burned the whole per-bucket budget on one bucket and then
gave up on all 16 seeds. A multiplication has no such fixed point: as `p`
varies, `(h1 - h2) * p` sweeps the whole word. After the change the *first*
seed succeeds on every sample measured below, with 7 to 21 pilot trials per
type id.

Two configurations are built, differing only in their template arguments:

| variant | Lambda | LoadPercent | slots | pilots | total |
|---|---|---|---|---|---|
| `hd` | 4 | 95 | 1.053N | N/4 x 4 B | **9.45 bytes per type id** |
| `hdmin` | 2 | 100 | N, exactly | N/2 x 4 B | 10.05 bytes per type id |

`hdmin` is the exactly minimal one. It costs *more* in total, which is the
useful thing to know: reaching a full table needs the buckets halved, and that
doubles the pilot array, which is dearer than the 5 % of slots it recovers.
The pilots cannot be narrowed to 16 bits instead - a pilot multiplies the key,
so a narrow one cannot move a bucket far enough.

### 11.2 Codegen

GCC 13.3, between the `type_info` load and the indirect call, Intel syntax:

```asm
imul   rax, QWORD PTR [rip+...]        ; seed
mov    rsi, rax
shr    rsi, cl                         ; bucket_shift
mov    edx, DWORD PTR [rdx+rsi*4]      ; the pilot
imul   rax, rdx
mul    QWORD PTR [rip+...]             ; slots, into rdx
mov    rax, QWORD PTR [rax+rdx*8]
```

Six instructions where `fast_perfect_hash` has three, and - the part that
matters - **two dependent loads instead of one**: the pilot has to arrive
before the index exists. clang 22 generates the same shape.

### 11.3 Memory: the first scheme here that beats the incumbent

Table plus pilots, on the samples of §4-5 and §10.1. `pext_index` is the
column to compare against for the four-module rows, since `fast_perfect_hash`
does not get that far.

| sample | N | `fast_perfect_hash` | `pext_index` | `hd` | `hdmin` |
|---|---|---|---|---|---|
| flat, 1 module | 1000 | 16 KB | 32 KB | **9 KB** | 10 KB |
| tree, 1 module | 1000 | 32 KB | 30 KB | **9 KB** | 10 KB |
| si-heavy, 1 module | 1000 | 128 KB | 128 KB | **9 KB** | 10 KB |
| mi-heavy, 1 module | 1000 | 128 KB | 128 KB | **9 KB** | 10 KB |
| si-heavy, 4 modules | 4000 | *aborts* | 448 KB | **37 KB** | 39 KB |
| mi-heavy, 4 modules | 4000 | *aborts* | 4.0 MB | **37 KB** | 39 KB |
| si-heavy, exe + `dlopen`ed | 4000 | *aborts* | 2.0 MB | **37 KB** | 39 KB |
| mi-heavy, exe + `dlopen`ed | 4000 | *aborts* | 2.0 MB | **37 KB** | 39 KB |

The `hd` column is one number, 9.45 bytes per type id, repeated eight times.
Nothing in the address distribution reaches it - not the shape mix, not the
module count, not `dlopen`. That is 1.7x to 14x below `fast_perfect_hash`
where the incumbent works at all, and 12x to 110x below the `pext` cover where
it does not. It is also the only scheme in this note whose footprint can be
stated in advance, before seeing an address.

### 11.4 Initialize: bounded, and indifferent to layout

`type_hash::initialize` alone, same protocol as §7 and §10.4, in ms. Each
`fast_perfect_hash` figure is one ASLR replicate and the choice of replicate is
worth up to 2.5x - see the note in §7, which also carries the corrected
`mi-heavy` range used below:

| sample | N | `fast_perfect_hash` | `pext_index` | `hd` | `hdmin` |
|---|---|---|---|---|---|
| flat, 1 module | 1000 | 0.199 | 1.3 | 0.068 | **0.029** |
| tree, 1 module | 1000 | 20.2 | 1.4 | 0.058 | **0.031** |
| si-heavy, 1 module | 1000 | 83.3 | 2.0 | 0.056 | **0.029** |
| mi-heavy, 1 module | 1000 | **110-175** (§7) | 2.3 | 0.062 | **0.034** |
| si-heavy, 4 modules | 4000 | *aborts* | 12.6 | 0.294 | **0.157** |
| mi-heavy, 4 modules | 4000 | *aborts* | 15.4 | 0.319 | **0.156** |
| si-heavy, exe + `dlopen`ed | 4000 | *aborts* | 17.1 | 0.277 | **0.151** |
| mi-heavy, exe + `dlopen`ed | 4000 | *aborts* | 20.6 | 0.323 | **0.155** |

Three to five thousand times faster than `fast_perfect_hash` - 3x on `flat`,
where the incumbent's search is trivial, and 1500x-5000x on the two samples
that cost it the most - 20 to 130 times faster than the cover search, and flat
across the samples: the search never looks at where the ids are, only at how
many. On the synthetic sweep, 65 536 ids take
3.1 ms (`hdmin`) to 8.5 ms (`hd`), and the stride - which moves
`fast_perfect_hash` from 0.5 ms to 44 s - moves `hd` by less than a factor of
two. `hdmin` is the faster of the two because its buckets hold two keys
rather than four, and a small bucket is far easier to place late.

### 11.5 Dispatch: this is where it is paid for

Same protocol as §6, ns per call, 64 registered classes, Zen 5.

All six variants were measured in one session, so the table also carries
`twolevel`, which belongs to §13; the machine drifts by about a nanosecond
between sessions and only figures taken together are comparable.

GCC 13.3:

| mode | classes | fph | pext | hd | hdmin | twolevel | direct |
|---|---|---|---|---|---|---|---|
| latency | 8 | 9.53+-0.04 | 9.37+-0.32 | 10.82+-0.31 | 10.91+-0.41 | 9.93+-0.38 | **9.23**+-0.31 |
| latency | 24 | 9.87+-0.12 | 9.59+-0.33 | 11.52+-0.34 | 11.33+-0.48 | 10.61+-0.38 | **9.52**+-0.31 |
| latency | 64 | 9.86+-0.18 | 9.79+-0.34 | 11.73+-0.44 | 11.76+-0.45 | 10.66+-0.50 | **9.65**+-0.36 |
| throughput | 8 | 8.84+-0.10 | 8.60+-0.26 | 10.88+-0.10 | 11.09+-0.20 | 9.71+-0.38 | **8.04**+-0.32 |
| throughput | 24 | 9.50+-0.12 | 9.06+-0.30 | 11.56+-0.14 | 11.67+-0.22 | 10.44+-0.43 | **8.43**+-0.36 |
| throughput | 64 | 9.55+-0.28 | 9.15+-0.33 | 12.00+-0.14 | 11.81+-0.19 | 10.68+-0.41 | **8.59**+-0.37 |

clang 22:

| mode | classes | fph | pext | hd | hdmin | twolevel | direct |
|---|---|---|---|---|---|---|---|
| latency | 8 | 9.03+-0.23 | 9.55+-0.24 | 10.64+-0.28 | 10.53+-0.28 | 10.50+-0.13 | **8.84**+-0.10 |
| latency | 24 | 9.49+-0.19 | 10.02+-0.24 | 11.21+-0.34 | 11.22+-0.29 | 10.89+-0.31 | **9.20**+-0.06 |
| latency | 64 | 9.63+-0.28 | 9.81+-0.28 | 11.30+-0.36 | 11.75+-0.23 | 11.07+-0.30 | **9.35**+-0.27 |
| throughput | 8 | 8.40+-0.18 | 8.63+-0.05 | 10.40+-0.31 | 10.38+-0.27 | 10.68+-0.15 | **8.33**+-0.11 |
| throughput | 24 | 8.90+-0.22 | 9.20+-0.69 | 11.07+-0.34 | 11.26+-0.17 | 11.29+-0.14 | **8.87**+-0.08 |
| throughput | 64 | 9.29+-0.22 | 9.32+-0.06 | 11.29+-0.28 | 11.70+-0.28 | 11.43+-0.16 | **8.75**+-0.11 |

Both compilers, every point: **+1.3 to +1.9 ns on latency, +2.0 to +2.5 ns on
throughput**, against `fast_perfect_hash`. That is not a marginal call. It is
two to four times the size of the effect the whole flat-index exercise was
chasing, in the wrong direction, and it is well outside the noise band.

Throughput suffers more than latency, which is the signature of instruction
count rather than dependency depth: the loop can overlap successive calls, so
what binds is the three extra uops and the second load port access, not the
chain. The two `hd` configurations are indistinguishable from each other, as
they should be - they generate identical code and differ only in table size.

### 11.6 Assessment

Against the §9 criteria: G1 passes - 180/180 (`ctest -R '^research-'`), nine
variants x two check modes x ten targets. G2 **fails**, and by more than any
other prototype here. G3 passes outright, and is the only prototype that does:
0.07x to 0.58x `fast_perfect_hash`, where A, B and C span 2x to 149x. G4 passes
with no restriction at all - the search never reads an address's magnitude.

D is the exact opposite trade to A and B - it buys with time what the flat
index buys with space - and both lose to the incumbent for the same reason,
that dispatch is the hot path and `initialize` runs once. The summary table
under *The five schemes* above puts all of them side by side.

What it is still good for is the same thing §10 identified, and it is a better
candidate for it than the cover:

- it is the natural **fallback** for `fast_perfect_hash`'s `search_error`,
  where the alternative today is `abort()`. It succeeds on both 4000-class
  samples, in 0.16-0.32 ms, in 37 KB;
- it needs **no instruction-set extension** - a 64-bit multiply and a shift -
  so unlike `pext_index` it is available on ARM and on pre-Haswell x86;
- it makes **no assumption about the type ids** beyond their being non-zero,
  so it is the only prototype here that does not depend on
  `policies::std_rtti` and the Itanium ABI. Zero is a fixed point of the
  multiply: a type id of 0 is pinned to slot 0 for every seed and every
  pilot, and the search fails if another bucket took that slot first. Hashing
  `x + 1` removes the restriction for one instruction on the dispatch path,
  which would matter if the policy were ever real.

The measured cost of that fallback is a permanently slower dispatch for the
programs that need it, which is a trade a program with 4000 classes across
four modules would take over aborting.

## 12. A per-module directory, keyed by the high bits (not built)

*No prototype was built for this one.* The table sizes below are measured, on
the same addresses as §4-5 and §10-11; the dispatch sequence is read off the
formula rather than off a compiler, and there are no timings. The memory
result is decisive on its own, and nothing in the scheme can move it.

R4 in §8 was dismissed on inspection, for the wrong reason - that recovering
the module from a pointer needs a range search. It does not. Hash the *high*
bits, which is a mult-shift like any other, and use the result to index a
directory of per-module coefficients:

```cpp
group = (x >> K) * M >> S;        // which module, in a few instructions
index = dir[group] + (x >> 3);    // that module's base, plus the flat index
```

Two steps, and no search: the second level is the flat index of §1, applied
once per module rather than once per program, so within a module the 16-byte
ABI separation still makes it injective. Folding the v-table table's address
and the module's minimum into `dir[group]` recovers §6's fused load, so the
dispatch sequence is

```asm
mov    rdx, rax
shr    rdx, K
imul   rdx, QWORD PTR [rip+...]        ; M
shr    rdx, S
mov    rdx, QWORD PTR [rip+...+rdx*8]  ; dir
mov    rax, QWORD PTR [rdx+rax*1]      ; fused
```

six instructions and two dependent loads - between `fast_perfect_hash` and
prototype D.

**The directory is free, and the second level is not.** Measured on the same
samples, at four modules and 4000 classes, under the `pie-split` placement -
an executable plus three `dlopen`ed libraries, the case that defeats §1:

| config | flat index | **two-step** | at `d = 16` | `pext_index` | `hd` | directory, `K = 12` |
|---|---|---|---|---|---|---|
| flat | 31.6 TB | 291 KB | 145 KB | - | 37 KB | 76 entries |
| tree | 31.6 TB | 812 KB | 406 KB | - | 37 KB | 178 entries |
| si-heavy | 31.6 TB | 3.1 MB | 1.6 MB | 2.0 MB | 37 KB | 192 entries |
| virt-heavy | 31.6 TB | 5.6 MB | 2.8 MB | - | 37 KB | 190 entries |
| mi-heavy | 31.6 TB | 8.7 MB | 4.3 MB | 2.0 MB | 37 KB | 196 entries |

The `dlopen` catastrophe is gone, completely and by construction - the span
between modules is never represented, so the two placement models give the
same figure to the byte. The directory that does it holds 76 to 196 entries
here, and never more than 417 on any configuration measured - under 4 KB - and
finding a perfect mult-shift over a few hundred values is work
`fast_perfect_hash` already does well. Both halves of the mechanism work.

What is left is **the sum of the per-module spans**, and that is the same
quantity §4 measured: 291 KB for `flat` is §4's 72 KB per module times four,
8.7 MB for `mi-heavy` is §4's 2.3 MB times four. §4 was not a fact about
having several modules. It was a fact about one, and this scheme inherits it
in full: 4.5x to 138x the 64 KB `fast_perfect_hash` would need for 4000
classes - it aborts on these samples, so that is the modelled size §4 uses -
and 8x to 240x prototype D's measured 37 KB. Halving it with `d = 16` costs
the fused load, as §8's R1 already found.

So the two-step index is **dominated on both axes at every measured point**.
`pext_index` is smaller on every sample where both were measured, by 1.6x
here and by 7x on `si-heavy` under the observed placement, where its cover
costs 448 KB against the same 3.1 MB - *and* faster, since it is one `pext`
and one load against six instructions and two dependent loads. Prototype D is
smaller again by two orders of magnitude. There is no setting of `K`, `M` or
`S` that changes this, because none of them touches the second level.

Two details are worth keeping even so:

- **The directory has to be keyed at page granularity.** `K = 12` leaves every
  group owned by exactly one module on every sample. `K = 16` does not: under
  the `pie-split` placement it shares a group between two modules on *all
  eight* configurations, because the loader packs `dlopen`ed libraries closely
  enough that two land in one 64 KB window, and the scheme cannot represent
  that. `K = 24` fails even under the observed placement. The obvious economy
  of a coarser key is the one thing that breaks it, and nothing detects it but
  an explicit check at `initialize` time.
- **A per-module *divisor* buys nothing.** The `d_max` column of `type-ids.md`
  is 16 on every configuration measured, so no module admits a coarser grid
  than the global one already does.

The variant with a per-module *mult-shift* instead of a per-module base fares
no better, for a different reason. Each module's sub-table would be
`fast_perfect_hash`'s usual 1.25n-2.5n, so the total lands back at the
incumbent's own size while still paying the extra load - and the search does
not get cheaper for being partitioned, since §7's worst case, 175 ms, is
already one module's 1000 classes.

## 13. Prototype E: two multiply-shifts, and a shift instead of a reduce

Prototype E, `research::two_level_index<Lambda, M1Candidates>`. An imperfect
multiply-shift picks a bucket; the bucket names a second multiplier; a second
multiply-shift produces the index:

```cpp
h     = m1 * x;                  // mix once
h1(x) = h >> s1;                 // which bucket, 2^b of them
h2(x) = (m2[h1(x)] * h) >> s2;   // the index, into 2^t slots
```

It is prototype D with one thing changed. D displaces multiplicatively too -
`mulhi(h * pilot, slots)` - and differs only in what turns the displaced key
into an index: D reduces with the top half of a product, onto a table of any
size, where this shifts, onto a table of a power-of-two size. Everything else,
including the bucketing and the largest-bucket-first placement, is the same
code shape. So the pair isolates exactly one variable.

### 13.1 The second multiply has to act on the mixed key

Written literally as `(m2[h1(x)] * x) >> s2`, on `x` itself, it does not work,
and it fails the way §11.1's exclusive-or did - structurally. Two type ids in
one module are 16 to 48 bytes apart, so `m2 * x` differs between them by about
`m2 * 16`; for a 32-bit `m2` that is below `2^(64 - t)`, the shift discards it,
and the two ids land on the same slot for every multiplier the search can try.
The search cannot fail visibly, because it can always grow the table instead -
which it did, to **2^24 slots for 1000 type ids** before it found an
assignment, a 134 MB table.

Multiplying the already-mixed `h` fixes it and costs nothing. `h` is ready
three cycles in, long before `m2` arrives from the directory, so the second
multiply is not waiting on it.

### 13.2 The shift is cheaper than the reduce, on one compiler

Between the `type_info` load and the indirect call, GCC 13.3:

```asm
imul   rax, rdx                        ; m1 * x
shr    rax, cl                         ; >> s1
mov    eax, DWORD PTR [rcx+rax*4]      ; m2[bucket]
imul   rax, rdx                        ; m2 * h
shr    rax, cl                         ; >> s2
mov    rax, QWORD PTR [rdx+rax*8]      ; the v-table pointer
```

Six instructions and two dependent loads, one step shorter than prototype D,
which needs `imul` plus `mul` to reduce. Measured in §11.5's table:

| | vs `hd` | vs `fast_perfect_hash` |
|---|---|---|
| GCC, latency | **-0.9 to -1.1 ns** | +0.4 to +0.8 ns |
| GCC, throughput | **-1.1 to -1.3 ns** | +0.9 to +1.1 ns |
| clang, latency | -0.1 to -0.3 ns | +1.4 to +1.5 ns |
| clang, throughput | +0.1 to +0.3 ns | +2.1 to +2.4 ns |

So on GCC it recovers about a nanosecond of prototype D's cost, and on clang
it recovers nothing. The difference is codegen, not architecture: GCC hoists
the shift amount out of the loop into `cl`, while clang reloads it on every
call as `movzx ecx, BYTE PTR [rip+...]` before `shr rax, cl`. Prototype D's
reduction has nothing to hoist - it is a single `mul` against a memory operand
- so it is the same on both. A policy that wanted this saving reliably would
have to make the shift a compile-time constant, which means fixing the table
size in the type.

Either way it is still slower than the incumbent's multiply and shift, on both
compilers and at every point.

### 13.3 Memory is a sawtooth

The table is `2^ceil(log2 N)` slots, so between 1.0 and 2.0 per type id, plus
a 32-bit multiplier per bucket. On the measured samples, `si-heavy` at four
sizes:

| N | slots | bytes per type id, `twolevel` | bytes per type id, `hd` |
|---|---|---|---|
| 1000 | 1024 | **9.22** | 9.45 |
| 2000 | 2048 | **9.22** | 9.45 |
| 3000 | 4096 | 12.29 | **9.79** |
| 4000 | 4096 | **9.22** | 9.45 |

Slightly cheaper than prototype D when the class count sits just under a power
of two, half as good again when it sits just over - the worst case, `N` one
above a power of two, is about 17 bytes per type id against D's flat 9.45.
Which of the two is smaller is decided by a property of the program that no
one controls.

One thing the power-of-two table does buy: when `N` *is* a power of two the
load factor is exactly 1, and the search still finds an assignment at
`Lambda = 4` - 65 536 type ids in 11.5 ms. That is an exactly minimal perfect
hash at twice `hdmin`'s bucket size and half its directory.

### 13.4 A per-bucket shift needs a per-bucket base, and then it partitions

`{m2, s2}` per bucket, rather than `m2` alone, only means something if each
bucket has a *private* sub-table - a per-bucket `s2` against a shared table
would just confine some buckets to a prefix of it, which is strictly worse.
Private sub-tables need a per-bucket base as well, `base[h1] + h2(x)`, and
that is the classical Fredman-Komlós-Szemerédi layout.

It costs more, and the reason is structural: sub-tables cannot lend each other
their slack, where in a shared table a bucket placed late fills the holes the
earlier ones left. Measured on the same samples, summing the smallest
power-of-two sub-table that holds each bucket:

| | shared table | FKS floor | FKS, sub-table `2k` |
|---|---|---|---|
| slots, `si-heavy` N=4000, `Lambda = 4` | **4096** | 5051 | 10102 |
| bytes per type id | **9.22** | - | 22.25 |

The floor - one slot per element, rounded up to a power of two per bucket, and
no room at all to find a perfect multiplier - is already above the shared
table, at every `Lambda` from 1 to 4. A sizing that leaves the search anything
to work with doubles that again, to 22-25 bytes per type id on every sample.
The directory grows too, from 4 bytes to 8 for the base and the shift, and the
dispatch path gains an unpack and an add. There is no configuration in which
partitioning wins.

### 13.5 Evening the buckets does not pay

`M1Candidates` first-level multipliers are scored by the sum of the squared
bucket sizes and the most even is kept. The metric does respond: over 64
multipliers on `flat` at 1000 ids, the sum ranges from 4252 to 10684 against
4000 for perfectly even buckets, and the largest bucket from 7 to 19.

The build time does not. Against the `twolevel1` variant, which takes the
first multiplier it is given:

| sample | 8 candidates | 1 candidate |
|---|---|---|
| flat, 1000 | 0.155 ms | **0.114 ms** |
| si-heavy, 1000 | **0.077 ms** | 0.082 ms |
| mi-heavy, 1000 | **0.095 ms** | 0.118 ms |
| si-heavy, 4000 | 0.404 ms | 0.404 ms |
| 65 536 synthetic | 11.5 ms | **11.0 ms** |

Noise, and at the large end the scoring passes are a small net loss. The
placement cost is not set by the largest bucket but by the tail, where every
remaining bucket faces an almost full table - and evening the sizes does not
change how full the table is. Prototype D reaches the same conclusion from the
other side: it never looks at `m1` at all and places just as fast.

### 13.6 Assessment

G1 passes - 180/180, now nine variants across two check modes. G2 fails,
though by less than prototype D on GCC. G3 passes on the same terms as D,
with the caveat that the sawtooth decides which of the two is smaller. G4
passes with no restriction.

It is a real refinement of prototype D, worth about a nanosecond where the
compiler hoists the shift, and it settles the design question the two of them
straddle: **the shared interleaved table is what makes either work**, and the
reduction that lets its size be arbitrary is worth its cycle or two precisely
because it removes the sawtooth. Neither reaches `fast_perfect_hash`.

## 14. A third compiler: MSVC

Everything above is GCC 13.3 and clang 22 on Linux. The whole harness was then
built and run with **MSVC 19.51 (Visual Studio 18 Build Tools)**, x64, Ninja,
Release, on the same Zen 5 machine. Three portability fixes were needed and
none of them touched a measurement: `pext_index` guarded its BMI2 requirement
on `__BMI2__`, which MSVC does not define, and used `__builtin_popcountll`;
`bench_dispatch` used a GCC `asm` barrier to keep a loop's result alive.

**It all works.** 180/180 tests pass - nine variants x two check modes x ten
tests - with no warnings from any of the 417 targets.

### 14.1 The tables are identical, and the incumbent's is not

Slots allocated for the same measured address sets:

| sample | `pext` | `hd` | `hdmin` | `twolevel` | `fast_perfect_hash` |
|---|---|---|---|---|---|
| flat, 1000 | 4096 | 1053 | 1000 | 1024 | **2048** vs 2047 |
| tree, 1000 | 3823 | 1053 | 1000 | 1024 | 4092 |
| si-heavy, 1000 | 16383 | 1053 | 1000 | 1024 | **16379** vs 16381 |
| mi-heavy, 1000 | 16384 | 1053 | 1000 | 1024 | **16370** vs 16381 |
| si-heavy, 4000 | 57343 | 4211 | 4000 | 4096 | *aborts* |
| mi-heavy, 4000 | 524218 | 4211 | 4000 | 4096 | *aborts* |
| si-heavy, exe + `dlopen`ed | 260970 | 4211 | 4000 | 4096 | *aborts* |
| mi-heavy, exe + `dlopen`ed | 261865 | 4211 | 4000 | 4096 | *aborts* |

Every prototype's column is **the same integer as on GCC**, on every sample.
They should be - each is a deterministic function of the addresses, and the
addresses come from a file - but it is worth having checked, because
`fast_perfect_hash`'s column is *not*. Its randomized multiplier search lands
on different factors under a different toolchain, and its table with it: 2048
against 2047, 16379 and 16370 against 16381. A program's v-table table is a
few kilobytes larger or smaller depending on which compiler built it, and
nothing announces the difference.

`initialize` costs the same order as on GCC and keeps the same shape: 0.03 ms
for `hdmin` at 1000 type ids against 104 ms for `fast_perfect_hash` on
`si-heavy`, which is the worst case there too. Both `index` and
`fast_perfect_hash` fail on exactly the samples they fail on under GCC -
`too_sparse` for the first, the multiplier search giving up for the second.

### 14.2 Codegen: the sequence is the designed one, plus a call

MSVC generates what §11.2 predicts, `hd` inside the throughput loop:

```asm
call        <dynamic type>                 ; not inlined
imul        rax, qword ptr [seed]
mov         r8, rax
mov         rcx, qword ptr [bucket_shift]
shr         r8, cl
mov         rdx, qword ptr [pilots]
mov         r8d, dword ptr [rdx+r8*4]      ; the pilot
imul        r8, rax
mov         rax, qword ptr [slots]
mul         rax, r8                        ; mulhi, into rdx
mov         rax, qword ptr [table]
mov         r8, qword ptr [rax+rdx*8]
call        qword ptr [r8+rax*8]
```

and the same shape for `fast_perfect_hash`, `imul` + `shr` + one load. The
`__umulh` intrinsic becomes the `mul` the `__uint128_t` spelling produces on
GCC, so the two really are the same instruction sequence.

The difference from both Unix compilers is the first line: **MSVC does not
inline fetching the type id**, where GCC and clang emit `mov rax,[rdi]` +
`mov rax,[rax-8]`. Every dispatch begins with an out-of-line call. That makes
the map a smaller share of the call, and it is why the differences between the
prototypes are compressed below.

### 14.3 Dispatch, within the MSVC build

Same protocol as §6, ns per call, 64 registered classes:

| mode | classes | fph | pext | hd | hdmin | twolevel | direct |
|---|---|---|---|---|---|---|---|
| latency | 8 | 4.47+-0.02 | **4.47**+-0.04 | 4.48+-0.02 | 4.48+-0.01 | 4.51+-0.03 | 4.48+-0.02 |
| latency | 24 | 4.54+-0.04 | 4.56+-0.03 | 4.55+-0.02 | **4.53**+-0.01 | 4.56+-0.01 | 4.54+-0.03 |
| latency | 64 | 4.65+-0.02 | 4.65+-0.01 | **4.65**+-0.01 | 4.66+-0.01 | 4.68+-0.02 | 4.69+-0.04 |
| throughput | 8 | 2.01+-0.03 | **1.99**+-0.03 | 2.23+-0.04 | 2.26+-0.11 | 2.04+-0.02 | 2.00+-0.03 |
| throughput | 24 | 2.00+-0.01 | **1.99**+-0.01 | 2.22+-0.03 | 2.25+-0.07 | 2.04+-0.05 | 2.00+-0.02 |
| throughput | 64 | 2.00+-0.03 | 2.01+-0.01 | 2.21+-0.03 | 2.24+-0.04 | 2.04+-0.03 | **1.99**+-0.05 |

**Do not read these against §6, §10.3, §11.5 or §13.2.** They are a different
operating system, a different allocator laying out the 4096 objects, and a
dispatch that starts with a call the other two inline; the absolute numbers
are not comparable and the ratio between platforms means nothing. What is
comparable is the column-to-column difference *within this table*:

| against `fast_perfect_hash` | latency | throughput |
|---|---|---|
| `pext` | -0.00 to +0.02 | -0.02 to +0.01 |
| `direct` | +0.00 to +0.04 | -0.00 to +0.00 |
| `twolevel` | +0.02 to +0.03 | +0.03 to +0.05 |
| `hd` | -0.01 to +0.01 | **+0.21 to +0.22** |
| `hdmin` | -0.01 to +0.01 | **+0.24 to +0.25** |

MSVC agrees with the other two compilers on the sign of the one result that
matters and on nothing else. The minimal perfect hashes cost measurably more
on throughput, and the effect is again larger on throughput than on latency -
the signature of instruction count rather than dependency depth, as §11.5
found. Everything else is inside the noise: the single-load prototypes buy
nothing here, where GCC and clang both showed them a few tenths of a
nanosecond ahead. With an un-inlined type id fetch in front of every dispatch,
there is less left for them to save.

### 14.4 One caveat the ABI does not cover

§2 derives the 16-byte separation of two `type_info` objects from the Itanium
ABI, and prototypes A and B rest on it. **MSVC is not that ABI**, so the proof
does not transfer, and nothing here replaces it. What can be said is that
`check_prototype`'s assertion - distinct `type_info` addresses at least 16
bytes apart, and `(x - base) / d` injective at `d = 8` and `d = 16` - passes
under MSVC as it does under GCC. That is one program's classes, not a
guarantee. A flat index shipped for Windows would need its own argument, and
`too_sparse` remains the only thing standing between it and a bad allocation.

## 15. Reading MSVC's type id inline

§14.2 left a loose end: MSVC does not inline fetching an object's type id, so
every dispatch begins with a call to `__RTtypeid` where GCC and clang emit two
loads. That is undocumented territory, but the layout is not secret - the
v-table is preceded by a pointer to a *complete object locator*, and that is
what `__RTtypeid` itself reads. `research/type_id_index/msvc_direct_rtti.cpp`
tries it. MSVC x64 only; it is not part of the CMake harness.

### 15.1 It works, and it is six instructions

On x64 the locator's fields are image-relative, so the image base has to be
recovered from the locator's own address first:

```cpp
auto vptr = *(const uint8_t* const*)obj;
auto col = *(const complete_object_locator* const*)(vptr - sizeof(void*));
return (const uint8_t*)col - col->self + col->type_descriptor;
```

which MSVC compiles to exactly what it looks like - here beside the call it
replaces, from the listing of the program's own verification loop:

```asm
mov  rax, QWORD PTR [rcx]        ; the v-table pointer
mov  r8,  QWORD PTR [rax-8]      ; the complete object locator
mov  eax, DWORD PTR [r8+20]      ; col->self, an RVA
mov  ebx, DWORD PTR [r8+12]      ; col->type_descriptor, an RVA
sub  rbx, rax
add  rbx, r8                     ; image base + RVA
call __RTtypeid                  ; what it is being checked against
cmp  rbx, rax
```

Two dependent pointer loads, then two 32-bit fields from one cache line and
two arithmetic instructions. The `signature` field is 1 and `self` exists only
on x64; on x86 the locator has no `self` and the other fields are already
pointers, so this spelling is 64-bit only.

### 15.2 Where it is wrong, and it is wrong silently

Over eleven shapes:

| shape | agrees with `&typeid(obj)` |
|---|---|
| single inheritance, as itself and as its base | yes |
| multiple inheritance, as itself | yes |
| multiple inheritance, as either base | yes |
| a class with a virtual base, seen as that base | yes |
| `VV : V, B`, as `VV`, as `A&`, as `B&` | yes |
| **a class with a virtual base, as itself** | **no** |
| **`VV : V, B` as `V&`** | **no** |

The rule is one thing: a class whose first word is a *virtual base table*
pointer rather than a v-table pointer. MSVC puts the vbtable pointer at offset
0 for a class with a virtual base, and the read then lands on the wrong word.
It does not fault. It produces a plausible address that is not a type
descriptor, so a method would dispatch to the wrong overrider or fail a lookup
that should have succeeded.

**Nothing catches it at compile time.** MSVC 19.51 at C++17 has neither
`std::is_virtual_base_of` nor `__builtin_is_virtual_base_of`, and there is no
other trait that distinguishes "leading word is a v-table pointer". The
library does know each class's bases - `use_classes` is given them - so a
registrar could refuse the classes this cannot handle; a `rtti` policy on its
own cannot. Under `runtime_checks` the honest form is to compute both and
compare, which costs nothing in a release build.

### 15.3 It is worth more than any map in this note

Measured in the shape a dispatch uses it - fetch the type id, multiply-shift
it, index a table - 4096 objects over 64 classes, median of 15 runs, three
repeats:

| | ns per fetch |
|---|---|
| `&typeid(obj)` | 1.10, 1.17, 1.14 |
| inline read | **0.60, 0.65, 0.67** |

About **half a nanosecond per dispatch**, and that number deserves to be put
next to §14.3's. Every difference between the six maps on MSVC was 0.02 to
0.25 ns, and the two minimal perfect hashes - the clearest effect in the whole
table - cost 0.21 to 0.25 ns. Inlining the type id fetch is worth twice the
largest of them, and it is worth it to *every* configuration at once.

That reframes §14.3 rather than adding to it. The prototypes look compressed
on MSVC because they are competing for a share of a call that spends a fixed
half-nanosecond before any of them is reached. On the platform where the map
matters least, the thing in front of it matters most - and it is the one part
of the sequence nobody was studying.

The follow-up has its own branch, `feature/msvc-direct-rtti`, carrying the
probe and a note of its own: what the layout is, the eleven shapes it was
checked against, and what excluding the unsafe one would take. Nothing here
depends on it.

It is also the only result in this note that suggests a change worth making to
the library rather than to a policy: `std_rtti::dynamic_type` could read the
locator directly on MSVC x64, for classes the registrar can prove have no
virtual base, and fall back to `typeid` otherwise. That is a bigger and more
delicate change than any of §§10-13, and unlike them it is not a research
question - it is a portability question about an undocumented layout that the
CRT relies on but the ABI does not promise.

## What to do with this

Nothing here replaces `fast_perfect_hash` as the general v-table lookup. Four
things survive the exercise, in the order they are worth doing:

1. **Open an issue for `fast_perfect_hash`'s search.** It is the one defect this
   note found in shipped code. 43-175 ms for 1000 classes on measured addresses
   (§7), seconds at 10^4-10^5, and on both 4000-class samples it gives up after
   500 000 attempts and calls `abort()`. The cost is structural - up to 5 passes
   x 100 000 attempts, each refilling a `2^M` bucket array and re-inserting
   every id (`fast_perfect_hash.hpp:213-277`) - and
   `test/test_slot_allocator.cpp:327-337` already notes it in passing. A second
   defect belongs in the same issue: the table size is a function of which
   toolchain built the program, because the multiplier search is randomized
   (§14.1), and nothing announces it.

2. **D is the candidate for that failure path, and for
   [#56](https://github.com/boostorg/openmethod/issues/56).** It succeeds on
   every input measured here, in 0.16-0.32 ms, in 37 KB, with no
   instruction-set requirement and no dependence on the Itanium ABI (§11.6).
   The price is a permanently slower dispatch - +1.3 to +2.5 ns - for the
   programs that select it, which is a trade a program with 4000 classes across
   four modules takes over aborting. C is the alternative and the weaker one: it
   needs BMI2, and its table is larger on every sample (§10.5).

3. **Read the type id inline on MSVC x64** (§15). This is the largest per-call
   effect measured anywhere in this note - about half a nanosecond, twice the
   biggest gap between any two maps on that platform - and it is the only result
   that suggests changing the library rather than a policy. It is also the most
   delicate: the read is silently wrong for a class whose first word is a
   virtual base table pointer, and no C++17 trait on MSVC detects that. The
   probe and its own note are on `feature/msvc-direct-rtti`.

4. **The injectivity theorem is reusable.** Any future scheme keyed on
   `std::type_info` addresses can rely on a guaranteed 16-byte separation under
   the Itanium ABI (§2) - though not under MSVC, where §14.4 has only a
   measurement and not a proof.

The harness itself is worth keeping as it stands: it re-runs nine registry
configurations against ten of the library's own tests, which is how each
prototype was shown to dispatch correctly and not merely quickly.

## Reproducing

MSVC, from WSL: write a batch file that calls `vcvars64.bat` (Build Tools 18,
under *Program Files (x86)*), then

```bat
cmake -G Ninja -B build-msvc -DCMAKE_BUILD_TYPE=Release ^
  -DBOOST_SRC_DIR=D:/boost -DBOOST_OPENMETHOD_BUILD_TESTS=ON ^
  -DBOOST_OPENMETHOD_BUILD_RESEARCH=ON
cmake --build build-msvc --target research-all -j 12
ctest --test-dir build-msvc -R "^research-" -j 12
dumpbin /disasm research-hd-bench_dispatch.exe
```

`cl.exe` cannot read `\\wsl.localhost` paths, so the checkout and the sample
files have to live on a Windows drive.

```bash
# data (separate repo)
cd ~/dev/typeinfo-samples
./make-samples.sh && ./collect-samples.sh
python3 analyze-type-ids.py --export-type-ids exported   # -> type-ids.csv, type-ids.md
# exported/ holds the observed placement plus one file per model in
# --placement; the `-pie-split` ones are the exe + dlopen case of §5, and are
# what the §11 tables measure.

# prototypes and measurements
cmake -B build-research -DBOOST_SRC_DIR=/path/to/boost \
      -DBOOST_OPENMETHOD_BUILD_TESTS=ON -DBOOST_OPENMETHOD_BUILD_RESEARCH=ON \
      -DCMAKE_BUILD_TYPE=Release
capped -m 24G -- cmake --build build-research --target research-all -j4  # see the appendix
ctest --test-dir build-research -R '^research-'          # correctness
cmake --build build-research --target research-disasm    # codegen
build-research/research/type_id_index/research-{fph,index,index16,direct,pext,hd,hdmin,twolevel}-bench_dispatch
build-research/research/type_id_index/research-{fph,index,pext,hd,hdmin,twolevel,twolevel1}-bench_initialize

# §7's replicate table: one sample's four ASLR replicates, per variant. With no
# file arguments the same binary runs the synthetic sweep instead.
for v in fph pext hd hdmin; do
  for cfg in flat tree si-heavy mi-heavy; do
    build-research/research/type_id_index/research-$v-bench_initialize \
      ~/dev/typeinfo-samples/exported/$cfg-0000000{0,1,2,3}-1000.txt
  done
done
cd ~/dev/typeinfo-samples && python3 analyze-bits.py     # -> bits.md (§10)
python3 analyze-two-step.py                              # -> two-step.md (§12)
```

The harness is off by default (`BOOST_OPENMETHOD_BUILD_RESEARCH`), has no
Jamfile, and is not part of the library's API.

## Appendix: building the harness

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
