# Archived `type_info` addresses

The address sets every measured figure in
[`../../../TYPE_ID_INDEX.md`](../../../TYPE_ID_INDEX.md) is computed from, as
`../samples/` produced them. They are committed because **they cannot be
regenerated**: `samples/make-samples.sh` rebuilds the generated libraries
deterministically, but the addresses come from `dlopen`ing them under ASLR, so a
fresh collection gives different numbers and a different answer for any search
that depends on placement. Without these files the note's tables are not
checkable by anyone, including their author.

One address per line, lowercase hex, no `0x`, ascending - the format
`bench_initialize` and `bench_dispatch` read when handed file arguments:

```
7c918f4c97a0
7c918f4c97b0
7c918f4c97c8
```

Names are `<shape>-<observation>-<N>[-<placement>].txt`:

| field | values |
|---|---|
| `<shape>` | `flat`, `tree`, `si-heavy`, `mi-heavy`, `virt-heavy`, and `si-heavy-tu{10,100,1000}` for the translation-unit axis |
| `<observation>` | the 8-digit id of one `collect-type-infos` run - `00000000`-`00000003` are the four ASLR replicates at 1000 classes, `00000014` the four-module set at 4000 |
| `<N>` | registered classes: 1000 for the single-module rows, 4000 for the four-module ones |
| `<placement>` | absent for the placement as ASLR actually produced it; `adjacent` for modules packed together; `pie-split` for an executable plus `dlopen`ed modules - the case of §5 |

56 files, eight shapes x (four replicates at 1000 + three placements at 4000),
which covers every row in the note. The full collection is larger - every
translation-unit and module-count combination - and stays machine-local; see
`../samples/README.md` to reproduce it.
