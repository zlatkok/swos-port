# Animation Table Source Format

`src/game/animation/animationTables.in` describes sprite-frame sequences and the animation tables that
select a sequence for each player type and direction. `generateAnimationTables.py` validates this file and
emits C++ data.

Both `//` C++ comments and `;` assembly comments run to the end of the line. Whitespace and commas are
otherwise interchangeable separators.

## Frame tables

A frame table starts with an identifier followed by sprite indices and animation-control values. It ends
when the next entity identifier or the end of file is reached.

```text
playerRunningTopFrames 343, 341, 342, 341, kLastFrameLoopMarker
```

Sprite indices may be decimal integers or members of `SpriteIndices` from `src/sprites/sprites.h`. The
animation constants declared in `src/swos/swos.h` may also be used:

- `kLastFrameLoopMarker` (`-999`) restarts the animation.
- `kLastFrameHoldMarker` (`-101`) holds the final frame.
- Values below `kFrameLoopbackMarker` (`-100`) move backward within the frame table after adding 100. They
  may instead be written as `loopback N` or `loopback(N)`, where `N` is from 2 through 32668; for example,
  `loopback(4)` emits `-104`. The resulting jump must not precede the beginning of the frame table.
- Values from `-99` through `-1` change the frame delay. They may instead be written as `wait N` or
  `delay N`, where `N` is a positive frame count from 1 through 99. Parentheses are optional, so `wait 5`,
  `wait(5)`, `delay 5`, and `delay(5)` are equivalent and emit `-5`.

`export`, `wait`, `delay`, and `loopback` are reserved keywords and cannot be used as frame-table or animation-table
names.

A frame table must finish with a value at or below `-100`. The exact value `-100` is rejected because it
repeatedly selects itself without displaying a frame. Relative jumps must remain inside the table, and
every chain of immediate control values must eventually reach a sprite or `-101`; negative-only cycles are
rejected. The generator warns about tables containing no sprite indices and about elements following
`-999` or `-101`, which normal playback cannot reach.

A standalone frame table that runtime code accesses directly can be prefixed with `export`:

```text
export ballMovingFrameTable 1182, 1181, 1180, 1179, -999
```

The generated header then declares `int16_t getBallMovingFrameTableOffset()`. The returned offset remains
valid when duplicate or suffix folding changes the table's physical location. Exported production tables
must have a corresponding original SWOS table when generating the `SWOS_TEST` branch.

## Animation tables

An animation table consists of its identifier, braces, a frame delay in the unsigned 8-bit range (`0`
through `255`), and up to four sections:

```text
playerRunningAnimTable {
    5
    team 1 player { team1Top team1TopRight team1Right }
    team 2 player { team2Top team2TopRight team2Right }
    goalkeeper 1 { goalkeeper1Top goalkeeper1TopRight goalkeeper1Right }
    goalkeeper 2 { goalkeeper2Top goalkeeper2TopRight goalkeeper2Right }
}
```

An optional comma may follow the frame delay, so `animation { 5, ... }` is equivalent to
`animation { 5 ... }`.

Section names are also accepted without spaces (`team1player`, `goalkeeper2`). Missing sections and
direction slots are emitted as null pointers. Frame tables may be referenced before their definitions, but
every reference must resolve by the end of the file.

Directions occupy these slots:

0. `top`
1. `top-right`
2. `right`
3. `bottom-right`
4. `bottom`
5. `bottom-left`
6. `left`
7. `top-left`

An unqualified reference is placed in the first free slot. Explicitly qualified references are placed
first, regardless of where they appear in the section.

```text
goalkeeper 1 {
    left: divingLeft
    right: divingRight
}
```

Ranges include both endpoints and wrap from `top-left` to `top`:

```text
top-left to right: comingFromTop
bottom-right to left: comingFromBottom
```

A range with identical endpoints is accepted with a warning. Assigning a slot twice, supplying more than
eight frame tables, or combining `all:` with another reference is an error. `all:` fills all eight slots:

```text
team 1 player { all: losingReactionFrames }
```

A frame-table reference may be replaced with an inline frame table enclosed in square brackets. Inline
tables use the same values and validation rules as named frame tables and participate in shared-frame-data
deduplication:

```text
refWaitingAnimTable {
    5,
    all: [ 395, wait 5, 396, -999 ]
}
```

## Referee tables

An animation whose identifier contains `ref` is a referee table. It has one direction section directly
inside its outer braces instead of named player and goalkeeper sections:

```text
refWaitingAnimTable {
    5
    all: refWaitingFrames
}
```

## Generator

```text
python src/game/animation/generateAnimationTables.py INPUT HEADER SOURCE \
    --sprites src/sprites/sprites.h --swos src/swos/swos.h
```

The command writes a public header containing the packed animation-table format and one getter for every
animation table. Frame and animation table objects remain private to the generated C++ file.

All frame values are stored in one `int16_t` array. Exact duplicate frame tables share an offset, and a
table that is an exact suffix of a larger table points inside the larger table. Missing direction entries
use offset `0xffff`. Pass `--no-frame-table-folding` to store every frame table independently instead,
including exact duplicates and suffixes.

An animation table begins with an 8-bit delay and an 8-bit flags field. Each present section then
contributes eight 16-bit frame-table offsets in this order:

1. goalkeeper 1 (flag bit 0)
2. goalkeeper 2 (flag bit 1)
3. team 1 player (flag bit 2)
4. team 2 player (flag bit 3)

Referee tables set bit 4 and contain one group of eight offsets. `getFrameTableOffsets()` locates the
variable-length offset array following the two-byte header, and `getFrameTable()` resolves an offset into
the shared frame data.

Run `python src/game/animation/testGenerateAnimationTables.py` to execute the parser tests.
