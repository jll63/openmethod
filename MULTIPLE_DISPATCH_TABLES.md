# Multiple dispatch tables

How Boost.OpenMethod dispatches a method with two or more virtual parameters:
the shape of the tables it dispatches through, the code that reads them, and
how `initialize()` builds them without spending a cell on every combination
of classes.

Slot numbers are taken as given here. A slot is the index, in the v-table of
an argument's class, of the entry that belongs to one virtual parameter of
one method; how those indices are chosen is the subject of
`SLOT_ALLOCATION.md`. This document starts where that one ends: every
virtual parameter has a slot, and what remains is to decide what to put in
the entries, and what to put in the table those entries lead to.

The running example is the `approve` method from the documentation's
multiple dispatch chapter (`doc/modules/ROOT/examples/rolex/7/main.cpp`),
with `Salesman` added so that one group has two members on both axes:

```c++
struct Role { virtual ~Role() = default; };
struct Employee : Role {};
struct Salesman : Employee {};
struct Manager : Employee {};
struct Founder : Role {};

struct Expense { virtual ~Expense() = default; };
struct Public : Expense {};
struct Bus : Public {};
struct Metro : Public {};
struct Taxi : Expense {};
struct PrivateJet : Expense {};

BOOST_OPENMETHOD_CLASSES(
    Role, Employee, Salesman, Manager, Founder,
    Expense, Public, Bus, Metro, Taxi, PrivateJet);

BOOST_OPENMETHOD(
    approve, (virtual_ptr<const Role>, virtual_ptr<const Expense>, double),
    bool);

// #0
BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Role>, virtual_ptr<const Expense>, double),
    bool) { return false; }
// #1
BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Employee>, virtual_ptr<const Public>, double),
    bool) { return true; }
// #2
BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Manager>, virtual_ptr<const Taxi>, double amount),
    bool) { return amount < 100.0; }
// #3
BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Founder>, virtual_ptr<const Expense>, double),
    bool) { return true; }
```

The overrider numbers are the indices the compiler gives them, in
registration order. Every trace excerpt below comes from running this program
with `initialize(trace{})`; type names are shortened.

## The problem

A method with one virtual parameter dispatches like a virtual function. The
argument's v-table has a slot for the method, and the entry in that slot is a
pointer to the overrider to call. One load, one indirect call.

With N virtual parameters, the overrider depends on N dynamic classes at
once. The natural structure is an N-dimensional table with one axis per
parameter, indexed by the class of each argument; each argument's v-table
then only has to supply the position of its class along its own axis. That
keeps the lookup free of hashing and searching, but the table has a cell for
every combination of classes. For `approve` that is 5 roles times 6 expenses,
30 cells, laid out here with the rows indexed by the first parameter:

|            | Expense | Public | Bus | Metro | Taxi | PrivateJet |
|------------|---------|--------|-----|-------|------|------------|
| Role       | #0      | #0     | #0  | #0    | #0   | #0         |
| Employee   | #0      | #1     | #1  | #1    | #0   | #0         |
| Salesman   | #0      | #1     | #1  | #1    | #0   | #0         |
| Manager    | #0      | #1     | #1  | #1    | #2   | #0         |
| Founder    | #3      | #3     | #3  | #3    | #3   | #3         |

Thirty cells is nothing, but the size is a product: three parameters over
hierarchies of a few dozen classes each is tens of thousands of cells, and a
program with many such methods pays that for every one of them. Most of it is
copies. The `Employee` and `Salesman` rows are identical; so are the
`Expense` and `PrivateJet` columns, and the `Public`, `Bus` and `Metro`
columns.

The redundancy has an exact source. A cell holds the outcome of overrider
selection, and selection looks at one thing: which overriders are
*applicable* to the argument classes, meaning which overriders have, in every
position, a parameter class that is a base of (or the same as) the argument
class. Along one axis, two classes to which exactly the same overriders are
applicable are indistinguishable to the method. Whatever the other arguments
are, the same overriders are in the running, so the same one wins.

So the compiler partitions each axis into *groups* of classes with the same
applicable set, and indexes the table by group instead of by class. On the
first axis, `Employee` and `Salesman` collapse into one group; on the second,
`Expense` and `PrivateJet` do, and so do `Public`, `Bus` and `Metro`. The
table becomes 4 by 3:

|                        | {Expense, PrivateJet} | {Public, Bus, Metro} | {Taxi} |
|------------------------|-----------------------|----------------------|--------|
| {Role}                 | #0                    | #0                   | #0     |
| {Employee, Salesman}   | #0                    | #1                   | #0     |
| {Manager}              | #0                    | #1                   | #2     |
| {Founder}              | #3                    | #3                   | #3     |

The lookup is unchanged in cost: a v-table entry now gives the group of a
class rather than the class itself, and that is what indexes the table. What
changes is what the size depends on. Groups are induced by the overriders'
parameter classes, not by the hierarchy, so the table grows with the number
of overriders, not with the number of classes.

## What is in memory at run time

### Words

Everything the dispatcher reads is a `detail::word` (`preamble.hpp`), a
pointer-sized union with three views:

```c++
union word {
    void (*pf)();   // a function pointer: an overrider, or an error stub
    std::size_t i;  // an integer: a group index
    word* pw;       // a pointer to another word: into a dispatch table
};
```

V-tables, dispatch tables and the entries linking them are all arrays of
`word`. A v-table pointer (`vptr_type`) is a `const word*`.

### V-table entries

A class's v-table has one entry per slot, and a slot belongs to one virtual
parameter of one method. What the entry holds depends on the method's arity:

- For a method with a single virtual parameter, the entry is `pf`: the
  overrider selected for that class. There is no dispatch table.
- For a method with several virtual parameters, the entry for the *first*
  virtual parameter is `pw`: a pointer into the method's dispatch table, to
  the start of the row that belongs to the class's group. The entry for every
  *other* virtual parameter is `i`: the index of the class's group on that
  parameter's axis.

### The method object

Every method has a static object, `method::fn`, and it carries the numbers
the dispatcher needs (`core.hpp`):

```c++
std::size_t slots_strides[2 * Arity - 1];
```

The first `Arity` elements are the slots, one per virtual parameter, in
parameter order. The remaining `Arity - 1` elements are the strides of
dimensions 1 through `Arity - 1`. Dimension 0 has stride 1, which is not
stored; it is folded into the `pw` entries as explained next.

### The dispatch table

A method's dispatch table is a contiguous run of `word`s, each a `pf`. If
axis d has |G_d| groups, cell (g_0, g_1, ..., g_{N-1}) is at offset

    g_0 + g_1 * s_1 + g_2 * s_2 + ... + g_{N-1} * s_{N-1}

where s_1 = |G_0| and s_d = s_{d-1} * |G_{d-1}|. Dimension 0 varies fastest,
so the cells that share every coordinate but g_0 are consecutive. Since the
first parameter's v-table entry is a pointer, the `g_0` term is added once,
at initialization, into that pointer: it points at `table + g_0`, and the
dispatcher never sees `g_0` as a number.

### One block for everything

All of this lives in a single `std::vector<word>`,
`registry_state<Registry>::st.dispatch_data`. The dispatch tables of every
multi-method come first, then the v-tables of every class. The registered
`static_vptr` of each class points into the v-table part, and whatever takes
an object to its v-table at run time, a `vptr` policy such as `vptr_vector`,
or `inplace_vptr` storing the pointer in the object, starts from those
registered pointers. Nothing outside the block is needed at dispatch time
except the method object.

### The lookup

`method::resolve` (`core.hpp`) is two functions, one for the first virtual
parameter and one for each of the others. The non-virtual parameters are
skipped by the `if constexpr` on `is_virtual`:

```c++
auto method::resolve_multi_first(
    const ArgType& arg, const MoreArgTypes&... more_args) const {
    if constexpr (is_virtual<mp_first<MethodArgList>>::value) {
        vptr_type vtbl = vptr<...>(arg);
        std::size_t slot = this->slots_strides[0];
        auto dispatch = vtbl[slot].pw;   // row for this class's group
        return resolve_multi_next<1, ...>(dispatch, more_args...);
    } else {
        return resolve_multi_first<...>(more_args...);
    }
}

template<std::size_t VirtualArg, ...>
auto method::resolve_multi_next(
    vptr_type dispatch, const ArgType& arg,
    const MoreArgTypes&... more_args) const {
    if constexpr (is_virtual<mp_first<MethodArgList>>::value) {
        vptr_type vtbl = vptr<...>(arg);
        std::size_t slot = this->slots_strides[VirtualArg];
        std::size_t stride = this->slots_strides[Arity + VirtualArg - 1];
        dispatch = dispatch + vtbl[slot].i * stride;
    }

    if constexpr (VirtualArg + 1 == Arity) {
        return *dispatch;                // the cell: a pf
    } else {
        return resolve_multi_next<VirtualArg + 1, ...>(dispatch, more_args...);
    }
}
```

Per virtual argument: one load from its v-table. Per dimension after the
first: one multiply and one add, folded into addressing. Then one load of
the cell and one indirect jump. For a two-parameter call through
`virtual_ptr` arguments, gcc at `-O2 -DNDEBUG` emits, for

```c++
bool call(virtual_ptr<const Role> r, virtual_ptr<const Expense> e, double a) {
    return approve(r, e, a);
}
```

the following (`virtual_ptr` is `{vp, obj}`, so `rdi`/`rdx` hold the two
v-table pointers, `rsi`/`rcx` the object pointers, `xmm0` the amount; all of
them pass through to the overrider untouched):

```asm
mov  rax, QWORD PTR approve::fn[rip+104]   ; slots_strides[1]: slot of parameter 1
mov  r8,  QWORD PTR approve::fn[rip+96]    ; slots_strides[0]: slot of parameter 0
mov  rax, QWORD PTR [rdx+rax*8]            ; e.vp[slot 1]: group index, as word.i
imul rax, QWORD PTR approve::fn[rip+112]   ; * slots_strides[2]: stride of dimension 1
mov  r8,  QWORD PTR [rdi+r8*8]             ; r.vp[slot 0]: row pointer, as word.pw
jmp  [QWORD PTR [r8+rax*8]]                ; tail-call the cell
```

A Debug build (runtime checks on) adds a test of the registry's
`initialized` flag in front of this.

## Construction

`initialize()` runs the registry's `compiler` (`initialize.hpp`). Its
`compile()` step is `augment_classes`, `augment_methods`, `assign_slots`,
`build_dispatch_tables`; `install_global_tables` then calls
`write_global_data`, which turns the result into the memory block above. The
first two steps produce what the table builder consumes:

- For every class, `class_::transitive_derived`: the class itself and every
  class deriving from it, directly or not (`SLOT_ALLOCATION.md` calls this
  the class's *cone*; the trace prints it as `covariant`).
  `class_::is_base_of(x)` is membership of `x` in that set, so it is true for
  the class itself.
- For every method, `method::vp`: the class of each virtual parameter, in
  order; `method::overriders`, each with its own `vp`; and two synthetic
  overriders, `not_implemented` and `ambiguous`, whose `pf` are the method's
  error stubs (`fn_not_implemented` and `fn_ambiguous`, which report a
  `no_overrider` or `ambiguous_call` error through the registry's error
  handler and abort).

`augment_methods` also checks that each overrider's class in position d is in
the cone of the method's class in position d (otherwise `missing_base`), so
by the time the tables are built, every overrider parameter class is on its
axis.

`build_dispatch_tables` then processes one method at a time.

### Step 1: form the groups

For every axis d, every class c in the cone of `vp[d]` gets a *mask*: a bit
set over the method's overriders, with bit k set if overrider k is applicable
to c in position d, that is, if `overriders[k].vp[d]->is_base_of(c)`.

```c++
for (auto covariant_class : vp->transitive_derived) {
    bitvec mask(m.overriders.size());
    std::size_t k = 0;
    for (auto& spec : m.overriders) {
        if (spec.vp[dim]->is_base_of(covariant_class)) mask[k] = 1;
        ++k;
    }
    auto& group = dim_group[mask];       // group_map = std::map<bitvec, group>
    group.classes.push_back(covariant_class);
    group.has_concrete_classes |= !covariant_class->is_abstract();
}
```

The groups of an axis are the distinct masks, held in a `std::map` keyed by
mask, so a group's *index* is its rank in increasing mask order. That makes
the numbering deterministic regardless of the order in which classes were
collected; the order itself carries no meaning. The trace prints a mask with
bit 0, overrider #0, on the right:

```
make groups for param #0, class Role
    overriders applicable to Founder      -> mask: 1001      (#0, #3)
    overriders applicable to Employee     -> mask: 0011      (#0, #1)
    overriders applicable to Salesman     -> mask: 0011
    overriders applicable to Manager      -> mask: 0111      (#0, #1, #2)
    overriders applicable to Role         -> mask: 0001      (#0)
make groups for param #1, class Expense
    overriders applicable to PrivateJet   -> mask: 1001      (#0, #3)
    overriders applicable to Taxi         -> mask: 1101      (#0, #2, #3)
    overriders applicable to Public       -> mask: 1011      (#0, #1, #3)
    overriders applicable to Bus          -> mask: 1011
    overriders applicable to Metro        -> mask: 1011
    overriders applicable to Expense      -> mask: 1001
```

Sorting the masks gives the group indices:

| axis 0 | mask | classes                | axis 1 | mask | classes              |
|--------|------|------------------------|--------|------|----------------------|
| 0      | 0001 | Role                   | 0      | 1001 | PrivateJet, Expense  |
| 1      | 0011 | Employee, Salesman     | 1      | 1011 | Public, Bus, Metro   |
| 2      | 0111 | Manager                | 2      | 1101 | Taxi                 |
| 3      | 1001 | Founder                |        |      |                      |

Abstract classes are grouped like any other. `has_concrete_classes` is
recorded per group but only feeds the `concrete only` figure in the trace's
`dispatch table rank` line; it does not shrink the table.

### Step 2: compute the strides

```c++
std::size_t stride = 1;
for (std::size_t dim = 1; dim < m.arity(); ++dim) {
    stride *= groups[dim - 1].size();
    m.strides.push_back(stride);
}
```

For `approve`, the single stride is |G_0| = 4. For a three-parameter method
with 2, 3 and 2 groups on its axes the strides are 2 and 6, and the table has
12 cells.

### Step 3: write the group indices into the v-table entries

Each class in each group gets its group index recorded in the entry for
this parameter's slot:

```c++
for (auto cls : group.classes) {
    auto& entry = cls->vtbl[m.slots[dim] - cls->first_slot];
    entry.method_index = m.index;
    entry.vp_index = dim;
    entry.group_index = group_num;
}
```

`class_::vtbl` is the compiler's model of the class's v-table, an array of
`{method_index, vp_index, group_index}` triples starting at the class's first
used slot. It is turned into `word`s in `write_global_data`, where `vp_index`
decides whether the entry becomes a `pw` or an `i`.

### Step 4: enumerate the cells

The table is filled by a recursion over the axes, starting from the *last*
one, with the mask of every overrider as the initial candidate set:

```c++
bitvec all(m.overriders.size());
all = ~all;
build_dispatch_table(m, dims - 1, groups.end() - 1, all, true);
```

At each level, the function loops over the groups of its axis in index
order, narrows the candidate set to the overriders applicable in this
position, and recurses toward axis 0:

```c++
for (const auto& [group_mask, group] : *group_iter) {
    auto mask = candidates & group_mask;
    if (dim == 0) {
        ... select an overrider from `mask`, push it onto m.dispatch_table ...
    } else {
        build_dispatch_table(m, dim - 1, group_iter - 1, mask, ...);
    }
}
```

Two things follow from the shape of this loop.

The order in which cells are pushed onto `m.dispatch_table` is: for each
group of the last axis, for each group of the one before, ..., for each
group of axis 0. Axis 0 varies fastest, which is exactly the layout the
strides of step 2 describe. `m.dispatch_table` is the table, in memory order.

The mask that reaches axis 0 is the bitwise AND of one mask per axis, so it
is the set of overriders applicable in *every* position, which is the
definition of applicable to the cell. The intersection is exact because
every class in a group has the same mask on that axis: a cell computed for
the group is correct for each of its classes. This is the whole justification
for grouping, and it is why the result is not an approximation.

For `approve`, the trace shows the narrowing for the `{Manager}` row of the
`{Taxi}` column:

```
group 1/2 mask 1101            <- axis 1, group 2: Taxi
    group 0/2 mask 0101        <- 0111 & 1101: Manager row
    select best of:
        #0 bool (*)(vp<Role>, vp<Expense>, double)
        #2 bool (*)(vp<Manager>, vp<Taxi>, double)
    -> #2 bool (*)(vp<Manager>, vp<Taxi>, double)
```

### Step 5: select the overrider for a cell

The candidates of a cell are the overriders whose bit survived. Among them,
`select_dominant_overriders` keeps those not dominated by another candidate,
where `is_more_specific(a, b)` holds when `a`'s class is derived from `b`'s in
at least one position and derived from or equal to it in every other:

```c++
auto is_more_specific(const overrider* a, const overrider* b) -> bool {
    bool result = false;
    for (each position) {
        if (a_class != b_class) {
            if (b_class->is_base_of(a_class))      result = true;
            else if (a_class->is_base_of(b_class)) return false;
        }
    }
    return result;
}
```

This is the same rule as C++ overload resolution restricted to derived-to-base
conversions: better in one position, no worse in any. It is a strict partial
order, and the elimination loop leaves its maximal elements. Then:

- No candidate at all: the cell is `not_implemented`. Its `pf` is the
  `fn_not_implemented` stub, so a call landing there raises `no_overrider`.
- Exactly one survivor: that is the cell.
- More than one survivor: the cell is `ambiguous`, whose `pf` is the
  `fn_ambiguous` stub, unless the `n2216` option is on (below).

The matrix example from the documentation's ambiguity section shows the two
error cells. With overriders #0 `(Matrix, SparseMatrix)` and #1
`(SparseMatrix, Matrix)`, both axes split into `{Matrix, DenseMatrix}` (index
0) and `{SparseMatrix}` (index 1), and the 2 by 2 table is:

|                       | {Matrix, DenseMatrix} | {SparseMatrix} |
|-----------------------|-----------------------|----------------|
| {Matrix, DenseMatrix} | not implemented       | #0             |
| {SparseMatrix}        | #1                    | ambiguous      |

The top-left cell has mask `01 & 10 = 00`: nothing applies. The bottom-right
one has `11`: both apply, neither dominates.

### Step 6: the `next` pointer

When a cell has a unique winner `w`, the same pass computes what `next` should
call from inside `w`. The winner is removed from the candidates and the
selection runs again on what is left; the result, or `not_implemented` when
nothing is left, or `ambiguous` when several survive, is stored in
`w->next`. `write_global_data` later copies its `pf` into the static
`method::next<Function>` that `BOOST_OPENMETHOD_OVERRIDE`'s `next` refers to.

`next` is stored per overrider, not per cell, and the loop overwrites it
every time `w` wins a cell. That is sound because the value cannot differ
between cells. If `w` is the unique maximal candidate of a cell, every other
candidate is below it in the partial order, hence has, in every position, a
class that is a base of `w`'s, hence is applicable in *every* cell `w` is
applicable in. So the candidate set for `next` is always "the overriders less
specific than `w`", whatever the cell. The loop is just a convenient place to
compute a property of the overrider.

An overrider that wins no cell is never entered through the method, and its
`next` is left untouched.

### Step 7: `n2216`

With `initialize(n2216{})`, an ambiguous cell is resolved instead of being
made an error, following N2216's rules: if the method's return type is a
registered class (in which case every overrider's is, or `augment_methods`
would have reported `missing_class`), the survivors whose return type is a
base of another survivor's are dropped; whichever survivors remain, the last
one in overrider order is picked. The pick is unspecified but stable across
runs, as the option's documentation says. The cell still counts as ambiguous
in the report, and the trace marks it `(ambiguous)`. In the matrix example the
bottom-right cell becomes #1.

Under `n2216` the argument of step 6 no longer holds: the winner is one of
several maximal candidates, the other maximal candidates can differ from
cell to cell, and `w->next` ends up as whatever the last cell that chose `w`
computed.

### Report

Each method's `report` counts its `cells` (the product of the group counts,
zero for single-parameter methods), `not_implemented` cells and `ambiguous`
cells. The totals returned by `initialize()` add up the cells but count
*methods*: `not_implemented` and `ambiguous` in the total are the number of
methods with at least one such cell.

### The single-parameter case

A method with one virtual parameter goes through the same steps with N = 1:
its classes are grouped by mask, and `build_dispatch_table` runs on axis 0
alone, producing one selected overrider per group. The resulting
one-dimensional "table" is never written out. `write_global_data` copies
each group's `pf` straight into the v-table entries of the group's classes,
which is what makes a uni-method call a single load.

## Writing the memory

`write_global_data` allocates one `std::vector<word>` sized to hold every
multi-method's `dispatch_table` plus every class's `vtbl`, and fills it in
that order.

For each multi-method it stores the slots and strides into the method
object's `slots_strides` array, then copies the table, replacing each
`overrider*` by its `pf`. `m.gv_dispatch_table` remembers where the table
landed. (For a uni-method only the slot is stored.) If the same method was
instantiated in several modules, every copy's `slots_strides` receives the
same values; see the shared libraries chapter of the documentation.

Then the `next` pointers are written, as described in step 6.

Then the v-tables, one class after another. The class's registered v-table
pointer is set to `gv_iter - cls.first_slot`, so that indexing it with an
absolute slot number lands in the class's storage even when the class's first
used slot is not 0. Each entry is written according to its `vp_index`:

```c++
if (method.arity() == 1) {
    *gv_iter++ = method.dispatch_table[entry.group_index]->pf;
} else if (entry.vp_index == 0) {
    *gv_iter++ = std::uintptr_t(method.gv_dispatch_table + entry.group_index);
} else {
    *gv_iter++ = entry.group_index;
}
```

The middle case is where the stride of dimension 0 disappears: the pointer
already includes the group index.

Finally the policies are initialized (this is when `vptr_vector` reads the
`static_vptr`s and builds its hash table), and the new block is swapped into
`registry_state::st.dispatch_data`. On a re-initialization, the previous
block is freed only when `write_global_data` returns, after every v-table
pointer has been redirected into the new one; the registry's `initialized`
flag is cleared before compilation starts and set again only after the
install, so a build with runtime checks refuses to dispatch in between.

## The example, end to end

For `approve` the block has 23 words. Slots are 0 on both axes, since each
hierarchy has nothing else in it; `slots_strides` is `{0, 0, 4}`.

Words 0 to 11 are the dispatch table, axis 0 fastest, so the four cells of
each column are consecutive:

| offset | (g_0, g_1) | cell | overrider              |
|--------|------------|------|------------------------|
| 0      | (0, 0)     | #0   | (Role, Expense)        |
| 1      | (1, 0)     | #0   | (Role, Expense)        |
| 2      | (2, 0)     | #0   | (Role, Expense)        |
| 3      | (3, 0)     | #3   | (Founder, Expense)     |
| 4      | (0, 1)     | #0   | (Role, Expense)        |
| 5      | (1, 1)     | #1   | (Employee, Public)     |
| 6      | (2, 1)     | #1   | (Employee, Public)     |
| 7      | (3, 1)     | #3   | (Founder, Expense)     |
| 8      | (0, 2)     | #0   | (Role, Expense)        |
| 9      | (1, 2)     | #0   | (Role, Expense)        |
| 10     | (2, 2)     | #2   | (Manager, Taxi)        |
| 11     | (3, 2)     | #3   | (Founder, Expense)     |

Words 12 to 22 are the eleven one-entry v-tables. The `Role` hierarchy is
axis 0, so its entries are row pointers; the `Expense` hierarchy is axis 1,
so its entries are group indices:

| offset | v-table of  | entry (slot 0)   |
|--------|-------------|------------------|
| 12     | Role        | pw = &table[0]   |
| 13     | Employee    | pw = &table[1]   |
| 14     | Salesman    | pw = &table[1]   |
| 15     | Manager     | pw = &table[2]   |
| 16     | Founder     | pw = &table[3]   |
| 17     | Expense     | i = 0            |
| 18     | Public      | i = 1            |
| 19     | Bus         | i = 1            |
| 20     | Metro       | i = 1            |
| 21     | Taxi        | i = 2            |
| 22     | PrivateJet  | i = 0            |

Now `approve(alice, taxi, 36.0)` with `alice` a `Manager`:

1. `alice`'s v-table pointer is word 15. Slot 0 there: `pw = &table[2]`.
2. `taxi`'s v-table pointer is word 21. Slot 0 there: `i = 2`.
3. `&table[2] + 2 * 4 = &table[10]`: cell #2, `(Manager, Taxi)`, which
   returns `36.0 < 100.0`.

And `approve(bob, jet, ...)` with `bob` an `Employee`: `&table[1] + 0 * 4`
is cell 1, #0, `false`. The `Salesman` and `Employee` v-tables hold the same
pointer, and the `PrivateJet` and `Expense` ones the same index, which is
the redundancy of the 30-cell table removed at its source rather than in the
table.

## Properties and costs

**Size.** On axis d the number of groups is at most the number of distinct
masks, and a mask is determined by which overrider classes on that axis are
ancestors of the class. Under single inheritance a class's ancestors form a
chain, so its mask is determined by the *deepest* overrider class above it
(or none), and |G_d| is at most P_d + 1, where P_d is the number of distinct
classes the overriders use in position d. The "+ 1" is the group of classes
no overrider reaches, which does not occur when, as in `approve`, an
overrider is defined on the method's own parameter class. So the table is
bounded by the product of the (P_d + 1), a function of the overriders alone;
adding classes to the hierarchies does not enlarge it. Multiple inheritance
can add groups: a class inheriting from two overrider classes on the same
axis gets the union of their masks, which no other class need have.

**What "redundancy-free" guarantees.** Two groups on an axis always have
different applicable sets. It does not strictly follow that their rows differ:
the overriders that distinguish two masks might never win a cell, and if they
only ever produce ambiguities the two rows can coincide. That takes
overriders which are ambiguous with each other everywhere they apply, a
configuration with no legitimate use, and the compiler does not look for it.

**Construction time.** Forming groups costs one `is_base_of` lookup per
(class on the axis, overrider) pair, summed over the axes, plus one map
insertion per class. Filling the table costs one selection per cell, and a
selection is quadratic in the number of candidates of that cell, times the
arity for each comparison; the `next` computation repeats it once. Both
parts are bounded by the table size times a small polynomial in the number
of overriders, so the size argument above bounds construction time as well
as memory.

**Dispatch time.** As shown in the assembly: per virtual argument one load
of a slot from the method object and one load from the argument's v-table;
per dimension after the first, one load of a stride and one multiply; then
the cell load and the jump. The loads from the method object are independent
of the arguments and of each other, so they overlap. What is *not* in that
sequence is obtaining the v-table pointers: with `virtual_ptr` arguments
they are already at hand; from a plain reference each one is a hash lookup
in the `vptr` policy, which is where the cost of a call from plain references
goes, as measured in the performance chapter of the documentation.

**Seeing it.** `initialize(trace{})`, or `initialize(trace::from_env())` with
`BOOST_OPENMETHOD_TRACE=1` in the environment, prints every stage above:
masks per class, groups per axis, strides, the candidate narrowing and
selection for each cell, the table in memory order, and each v-table entry
with its method, parameter and group.

## Relation to Amiel, Gruber and Simon's compressed dispatch tables

The scheme above is, in its essentials, the one proposed by Amiel, Gruber
and Simon at OOPSLA '94 ("Optimizing Multi-Method Dispatch Using Compressed
Dispatch Tables") and formalised by Amiel, Dujardin and Simon in INRIA
report RR-2977 (1996), later published in TOPLAS 20(1), 1998, as "Fast
Algorithms for Compressed Multimethod Dispatch Table Generation". The
definitions quoted here are from the report.

Their starting point is the same full table, one axis per argument, indexed
by type. They compress it by two rules (their section 3.2): eliminate an
entry of a dimension when every cell in its hyperplane is empty, and group
two entries of a dimension when their hyperplanes are identical, cell for
cell. The sets of types that share an index in a dimension are called
*index-groups*. The contribution of the paper is to compute the index-groups
directly from the method signatures, without building the full table, and
the device for that is the *pole*.

For argument position i of generic function m, with `⪯` the subtype
relation:

- `Static_m^i` is the set of types used as i-th formal argument by the
  methods of m (Def. 4.3), and `Dynamic_m^i` is the union of their cones
  (Def. 4.4).
- A type T is an *i-pole* if T is in `Static_m^i` (a *primary* pole) or if
  more than one pole is minimal among the poles strictly above T (a
  *secondary* pole, Def. 4.5).
- The *influence* of a pole T is the set of types below T for which T is the
  unique closest pole (Def. 4.7). Influences partition `Dynamic_m^i`
  (Prop. 4.2), and `pole_m^i(T)` names the pole whose influence holds T
  (Def. 4.8).
- Theorem 1: a type outside every influence can be eliminated from the
  dimension. Lemma 4.2 and Theorem 2: replacing each argument type by its
  pole does not change the selected method, so T and `pole(T)` can be
  grouped. The compressed table is therefore indexed by poles, and an
  influence is an index-group.
- Corollary 4.1: under single inheritance every pole is primary, so
  `Pole_m^i = Static_m^i`.

At run time (their section 3.3) each argument position has an
*argument-array*, indexed by a global type id, holding the index of the
type's pole, and the call reads `D[arg_1[t_1], ..., arg_n[t_n]]`.
Argument-arrays of different methods are then *colored* (section 3.4):
two *selectors* (a method and a position) conflict when their type sets
intersect, and non-conflicting ones share one array.

The correspondence, term by term:

| Amiel et al.                                    | OpenMethod                                   |
|-------------------------------------------------|----------------------------------------------|
| `Static_m^i`                                    | the overrider classes in position d          |
| `Dynamic_m^i`                                   | the classes with a non-empty mask            |
| primary i-pole                                  | an overrider's parameter class               |
| secondary i-pole                                | no counterpart (see below)                   |
| influence of a pole, index-group                | group                                        |
| `pole_m^i(T)` in the i-th argument-array        | group index in the class's v-table entry     |
| argument-array coloring                         | slot allocation                              |
| grouping condition (2), identical hyperplanes   | the sense of "redundancy-free" above         |
| fill-up: one MSA computation per tuple of poles | one selection per tuple of groups            |

Under single inheritance the correspondence is exact. An influence is then
"the types whose deepest overrider-class ancestor is T", and that is the
same partition as equal applicable-overrider sets, which is what the masks
compute. Four things differ.

**The grouping criterion under multiple inheritance.** Take A and B
unrelated, both overrider classes in position d, and two sibling classes E
and F each deriving from both. For the paper, E and F are two secondary
poles with two separate influences. Here they have the same mask and form
one group. The paper needs the finer split because it assumes only
argument-subtype precedence and monotonicity (their Definitions 2.2 and 2.3)
and lets the language supply the rest of the order, which may depend on the
invocation's types, as CLOS-style linearisation does; E could then
legitimately select the overrider on A while F selects the one on B.
`is_more_specific` looks only at the overriders' own classes and a tie is an
ambiguity, so an equal applicable set forces an equal outcome and merging E
and F is sound. The groups are the coarsest partition that is correct for
this library's semantics; poles stay correct for any monotonic precedence.

**Elimination.** Their Theorem 1 drops from a dimension the types with no
applicable method; a call with such an argument is a type error in a
statically typed language, or a null check in a dynamically typed one. Here
those classes stay, as the empty-mask group whose cells are
`not_implemented`, because the axis is the declared parameter's whole cone
and such a call has to reach the error stub.

**How the partition is computed.** They compute `pole(T)` in a single pass
over the type graph from each type's closest poles, in O(|m| + |Θ| + E)
for a method with |m| overriders, |Θ| types and E edges (their section 5).
Here a mask is computed per class by testing every overrider, and classes
are grouped by mask in a map. Different algorithm, same answer under single
inheritance.

**Where the index lives.** Their argument-arrays are per-selector arrays
indexed by a global type id, shared through coloring. Here the index sits in
the class's own v-table at the parameter's slot, reached through the
argument's v-table pointer, and the first dimension's index is folded into a
row pointer. The coloring rule, "selectors conflict when their type sets
intersect", is the slot-sharing rule of `SLOT_ALLOCATION.md`: two
parameters may share a slot number only if their cones are disjoint.

The report's related-work section notes that Chen, Turau and Klas
(ECOOP '94, "Efficient dynamic look-up strategy for multi-methods") compress
their lookup automata "using a notion very similar to our poles", so the
idea was in the air in 1994.

References:

- E. Amiel, O. Gruber, E. Simon. Optimizing Multi-Method Dispatch Using
  Compressed Dispatch Tables. OOPSLA '94, pp. 244-258.
  <https://dl.acm.org/doi/10.1145/191081.191117>
- E. Amiel, E. Dujardin, E. Simon. Fast Algorithms for Compressed
  Multi-Method Dispatch Tables Generation. INRIA RR-2977, 1996.
  <https://inria.hal.science/inria-00073721>
- E. Dujardin, E. Amiel, E. Simon. Fast Algorithms for Compressed
  Multimethod Dispatch Table Generation. ACM TOPLAS 20(1), 1998, pp. 116-165.
  <https://dl.acm.org/doi/10.1145/271510.271521>
