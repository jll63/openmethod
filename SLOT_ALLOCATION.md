# Slot allocation

How Boost.OpenMethod assigns v-table slots to method parameters, why the
allocator was reworked for issue #19, and what the rework measured against
the allocator it replaces.

## The problem

Every virtual parameter of every method needs a *slot*. When a method is
called, the dispatcher reads `vptr[slot]` from the v-table of the argument's
dynamic class, so the slot of a parameter declared on class `X` is occupied
in the v-table of `X` and of every class deriving from `X`. Call that set of
classes the *cone* of `X` (in the compiler it is `transitive_derived`, which
includes `X` itself).

Two parameters may share a slot number only if no v-table has to hold both,
that is, if the cones of their classes are disjoint. Otherwise some class
inherits both, and its v-table cannot store two entries at one index.

A v-table is stored as a contiguous array spanning the class's *first* used
slot to its *last* used slot. Leading unused slots cost nothing: the class's
v-table pointer is set to `storage - first_slot`, so indexing with the
absolute slot number still lands on the right entry. Trailing unused slots
do not exist, since the span ends at the class's own highest slot. What does
cost memory is a *hole*: an unused slot between two used ones. The total
memory spent on v-tables is therefore the sum over all classes of
`last_slot - first_slot + 1`, and that sum is what the allocator should keep
small.

Two more things frame the design:

- The order in which classes are registered is not under a program's
  control. It is the order of static construction across translation units,
  and across shared libraries. An allocator may not rely on it for
  correctness, and the less its result depends on it, the better.
- Single inheritance is the common case and must stay optimal: in a tree,
  every v-table should be dense from slot 0, with the slots of a class
  following those of its ancestors.

## The old allocator

The previous `assign_slots` had two code paths, chosen per root class (a
class with no bases). If no class in the root's cone had more than one
direct base, the whole cone was a tree and `assign_tree_slots` handled it.
Otherwise `assign_lattice_slots` did.

**Trees.** A depth-first walk from the root. Each class receives consecutive
slots starting right after its parent's last slot, so a class's v-table is
exactly as long as the number of parameters on it and its ancestors, from
slot 0. Siblings reuse the same numbers. This is optimal and was kept as a
requirement for the new allocator.

**Lattices.** Two bit sets per class:

- `used_slots`: the slots occupied in the class's v-table, its own
  parameters and those inherited from bases;
- `reserved_slots`: the slots the class must not take because some class
  *below* it already holds them through another base.

The walk was again depth-first from each root through `direct_derived`,
guarded by a mark so a class with several bases was allocated once, when
first reached. For each parameter on a class `X`:

1. `unavailable = used_slots[X] | reserved_slots[X]`;
2. the slot is the *lowest* free one, scanning from 0;
3. the bit is set in `used_slots[X]` and `reserved_slots[X]`;
4. `used_slots[X]` is merged into `reserved_slots` of every transitive base
   of `X`;
5. `used_slots[X]` is merged into `used_slots` of every class in the cone of
   `X`, and then into `reserved_slots` of every transitive base of each of
   those classes.

Step 5 is where a slot taken by `X` reaches the classes that share a
descendant with `X` but are not related to it otherwise: for `C : A, B`,
after `A` takes a slot, `C`'s v-table holds it, and `B`, a base of `C`, is
told to keep away from it. After all this, a class's v-table was sized from
its first used bit to its last.

Four things were wrong with it, all noted in issue #19:

1. **The lowest free slot leaves holes.** Once leading unused slots are
   skipped, the cheapest place for a new slot is next to the class's current
   range, not at the bottom of the numbering. Take `A1, B1, C1 : A, B,
   E1 : B` (a digit is the number of parameters on the class). Visiting `A`,
   `C`, `B`, `E` in that order, `A` gets 0, `C` gets 1, and `B` gets 2
   because `C`'s v-table already holds 0 and 1. `E` inherits `{2}` from `B`
   and takes the lowest free slot, 0: its v-table spans `[0..2]`, three
   entries for two parameters, with a hole at 1. Slot 1 or 3 would have
   given it two entries.
2. **Depth-first order visits a multiply-derived class before its other
   bases**, so the class takes slots its later bases then have to work
   around. The result depended heavily on which root came first in the
   registration order.
3. **Whole bit sets were merged** at every step, when only the slot just
   assigned is new.
4. **Reservations were pushed up** from the cone of the class to the bases
   of every class in that cone, a triple loop, to prepare bit sets that
   would only be read much later, if at all.

## The new allocator

### Pull the forbidden set from the cone

A slot `s` given to a parameter of `X` must be avoided by `Y` exactly when
some class `Z` inherits from both `X` and `Y` (`Z` may be `X` or `Y`
itself). `Z` is in the cone of `Y`, and `used_slots[Z]` contains `s`, because
`s` was propagated to the whole cone of `X` when it was assigned. Conversely,
any slot found in `used_slots` of a class in the cone of `Y` was put there
by some ancestor of that class, which shares that descendant with `Y`. So

    unavailable(Y) = union of used_slots[Z] for Z in cone(Y)

is exactly the set of slots `Y` may not take, neither more nor less, and it
can be computed when `Y` is allocated, from information the cone already
holds. `reserved_slots` is gone, and with it the push loops. After a slot is
assigned, one bit is set in `used_slots` of every class in the cone, and
that is all the bookkeeping there is.

This also means correctness no longer depends on the order in which classes
are allocated, only the compactness of the result does, which is what makes
the next two choices free.

### Allocate widest cone first

A parameter's slot must be free in every v-table of its class's cone, and
its cost is spread over all of them. Early on, the v-tables are empty or
alike and a slot that suits all of them is cheap; once they have grown apart
through other bases, a slot free in all of them and adjacent to each is
rare. So the classes whose slot must fit into the most v-tables go first:
the classes are sorted by decreasing cone size.

A base's cone strictly contains its derived classes' cones (it also contains
the base itself), so this order puts bases before derived classes, which is
what the "extend the inherited range" rule below needs. Between unrelated
classes with cones of the same size, the deeper one (more transitive bases)
goes first: it has an inherited range to extend and fewer options, whereas
a shallow class can go anywhere. Only ties left after that are broken by
registration order, which is the one place where it still has an influence.
A stable sort makes that deterministic.

Why not "bases first, deepest ready class first", the obvious refinement of
the issue's suggestion? Because the allocation order then depends on
registration order whenever several roots are ready, and the result showed
it: on the Django hierarchy below, the total swung from 261 to 314 across
registration orders. Sorting by cone size is structural, and the total
became the same in every order.

### Pick the slot that grows the v-tables the least

The cost of giving slot `s` to a parameter of `X` is the number of entries
it adds to the v-tables of the cone of `X`:

    cost(s) = sum over Z in cone(X) of growth(Z, s)

    growth(Z, s) = 1                 if Z has no v-table yet
                 = 0                 if first(Z) <= s <= last(Z)   (a hole)
                 = s - last(Z)       if s > last(Z)
                 = first(Z) - s      if s < first(Z)

The free slot with the smallest cost wins; the lowest slot on ties. This is
the exact increment of the memory measure defined above, not a proxy for
it, and it covers the three situations listed in the issue with a single
rule:

- a hole in `X`'s own range costs nothing anywhere, since every v-table in
  the cone contains that range;
- growing beyond either end of `X`'s range costs one per slot in `X` and in
  each class of the cone whose range does not already extend that far, so
  the cheaper direction wins and a descendant's range counts (for `X` at
  `{5}` with a descendant at `{0,1,2,3,5}`, slot 4 fills the descendant's
  hole and costs 1, slot 6 costs 2);
- a class with no inherited range lands inside a hole shared by its
  descendants, or next to their ranges, rather than at the lowest number.

Each term is flat inside a range and grows by one per slot away from it, so
the sum is convex and piecewise linear, and it is computed for every
candidate at once by a sweep from slot 0: `cost(0)` is the sum of the
`first(Z)` plus the number of empty classes, and stepping from `s` to `s+1`
adds one per range that ends at or before `s` and removes one per range
that starts after `s`. With two histograms of range starts and ends, the
sweep costs O(cone + slots) per parameter. The candidates are the free slots
in `[0, S]`, where `S` is one past every used slot and therefore always
free.

### The rest

The class is allocated one parameter at a time; `unavailable` is pulled
once per class and updated with each slot taken, since only the class's own
assignments change it meanwhile. The sizing pass is the same for every
class: no used slot, no v-table; otherwise `first_slot` is the first used
bit and the v-table spans to the last. The tree special case is gone: for a
tree, bases first plus cheapest growth reproduces its layout exactly, dense
from 0 with siblings reusing numbers, so one code path serves both.

Per class the pull costs O(cone × slots / 64) bit operations; per parameter
the sweep O(cone + slots) and the propagation O(cone). The old allocator
did O(bases + derived × bases) bit-set merges per parameter.

### The two examples again

`A1, B1, C1 : A, B, E1 : B`. Cones: `B` {B, C, E}, `A` {A, C}, `C` {C},
`E` {E}. `B` goes first and takes 0. `A` sees 0 held by `C` and takes 1.
`C`, inheriting 0 and 1, takes 2. `E`, inheriting 0, takes 1. Totals: 1, 1,
3, 2, that is 7, in every registration order; the old allocator gave 7 or
8 depending on the order.

`A1; B1, C1 : virtual A; D1 : B, C`. `A` (cone of 4) takes 0. `B` and `C`
have cones of two; say `B` goes first and takes 1. `C` cannot take 1, which
`D` holds, and grows to 2, with an unavoidable hole at 1. `D` takes 3.
Totals 1, 2, 3, 4: 10. The old allocator, allocating `D` before `C` in its
depth-first walk, gave `C` slot 3 and a hole in `D` as well: 11.

## Measuring the two

The comparison uses the test file `test/test_slot_allocator.cpp` as the
instrument. The tests do not assume a registration order. A hierarchy is
described as one registration per class, with its direct bases, and the
harness constructs those registrations as local objects in a chosen order,
runs `initialize`, checks, and destroys them, for every permutation of the
classes when there are at most seven, or for 66 fixed orders otherwise (the
listed order, its reverse, and 64 shuffles from a seeded generator, spelled
out so every platform draws the same ones). Every run checks that no two
parameters share a slot in any v-table, that every v-table starts at its
first used slot and ends at its last, and records the total v-table size.

The old numbers come from the same test binary compiled against the
previous `initialize.hpp`, so the same hierarchies, orders and checks apply
to both allocators. (One trap: the test compile command force-includes a
precompiled header that already contains the current `initialize.hpp`, so
an `-I` pointing at the old one is silently ignored unless that flag is
removed. The first attempt produced identical numbers for that reason.)

Five real hierarchies are exercised, each with one unary method per class:

| hierarchy | classes | source |
|---|---|---|
| ISO C++ stream classes, `basic_ios` virtual, plus the stream buffers | 20 | ISO C++, [iostream.forward.overview] |
| CPython `socketserver`, two mix-ins over four servers, plus the handlers and a binary method across the two trees | 18 | `Lib/socketserver.py`, PSF License 2.0 |
| Django class-based generic views, `ContextMixin` virtual | 45 | `django/views/generic/*.py`, BSD-3-Clause |
| ANSI Common Lisp standardized condition types, `condition` and `error` virtual | 30 | ANSI CL, section 9.1.1 |
| CPython `collections.abc`, `Iterable` and `Sized` virtual | 26 | library reference, PSF License 2.0 |

## Results

Total v-table size, as defined above, over all registration orders tried:

| hierarchy | old, min to max | new, every order |
|---|---|---|
| tree `A1; B1, C1 : A; D1 : B` | 8 | 8 |
| `A1, B1, C1 : A, B` | 5 | 5 |
| hole avoided `A1, B1, C1 : A, B, E1 : B` | 7 to 8 | 7 |
| virtual diamond `A1; B1, C1 : virtual A; D1 : B, C` | 11 | 10 |
| diamond plus `E3 : C` | 16 | 15 |
| grow down, 9 classes | 20 to 23 | 20 |
| cone-aware direction, 12 classes | 51 to 55 | 51 |
| ISO C++ streams | 95 to 97 | 88 |
| CPython socketserver | 76 to 84 | 77 |
| Django generic views | 298 to 420 | 303 |
| Common Lisp conditions | 131 to 143 | 126 |
| collections.abc | 102 to 114 | 96 |

What the table says:

- **The new total is the same in every registration order**, for every
  hierarchy, including the five real ones over 66 orders each. The old total
  varied with the order, by up to 41% on the Django views.
- **Trees and single-inheritance parts are unchanged**, as required.
- **On every diamond-carrying hierarchy the new allocator beats the old
  one's best order**: the streams by 7 entries, the Lisp conditions by 5,
  `collections.abc` by 6, the diamond examples by one each.
- **The greedy is not an optimum.** On `socketserver` and on the Django
  views, the old allocator's single best order beats the new total, by 1 and
  5 entries respectively, out of 77 and 303. On `socketserver` the better
  layout allocates the two mix-ins (cones of five) before `UDPServer` (cone
  of six): `UDPServer` then carries a hole, but the four Unix datagram
  classes below it each save an entry. Widest-cone-first cannot see that
  trade, and neither could the old allocator; it found it by chance in some
  orders. Both cases are recorded in the test comments.

Beyond the numbers, the rework removes one of the two bit sets, the tree
special case and a triple loop, and its result can be reasoned about: a slot
is free for a class iff no class in its cone holds it, and the slot chosen
is the one that adds the fewest v-table entries.

## Possible refinements

- The remaining order dependence is the tie between unrelated classes with
  cones of the same size and the same depth. A structural tie-break, for
  instance on the number of parameters or on the size of the union of the
  cone's ranges, would remove it, at the price of another rule to explain.
- The `socketserver` case suggests a bounded lookahead: when two ready
  classes have overlapping cones, try both orders for that pair and keep the
  cheaper. Whether the gain is worth the code is an open question; the
  present rule already sits within 2% of the best order found for the old
  allocator on that hierarchy, with none of its variance.
