# `ida2asm`

`ida2asm` is a tool for converting pseudo-assembly output from IDA Pro into
something that can actually be assembled/compiled without errors.

Even though it started as an assembler conversion tool it can now generate C++ output in addition to the
basic MASM x86 format. Having multiple output formats was an idea from the start so it was designed to be
easily augmented.

Aside from conversion, second task is symbol manipulation (importing, exporting, removing, etc.). This is
specified via a custom format input text file.

## Command-line parameters

`ida2asm` expects 6 command line parameters, in the following order:
- path to the exported input ASM file from IDA
- directory path for output files (it will be extracted if a file path is given)
- path to input control file
- path to output C++ header with exported symbols
- output format (currently only `masm` and `c++` are supported)
- number of files to split output ASM files (as well as how many threads to use), maximum is 20

Optional parameters are also accepted, they start with two dashes, and are currently only valid for
C++ output:

`--extra-memory-size=<size>` - statically allocate memory of given size

`--disable-optimizations`

Short help can be listed by passing `-h` or `--help` parameter.

## Input file

Input file controls how the conversion to actual assembly files will be performed. It consists of sections
which begin with `@` sign. Following sections are supported:
- `@import`
- `@export`
- `@remove`
- `@null`
- `@replace`
- `@save-regs`
- `@on-enter`
- `@insert-hook`
- `@type-sizes`

Any line beginning with `#` sign is considered a comment and will not be processed.

### `@import` section

Purpose of `@import` section is to replace assembly procedures with functions from C++. Any non-empty line
is treated as a procedure name that will be imported from C++. C++ functions do not take any parameters and
by default return `void`, unless symbol name is followed by a comma followed by `int` -- in that case return
type is `int`. Assembly procedure body is removed from generated assembly code, and its name is treated as an
extern label. All the imported C++ functions are listed in the generated header file, declared as
`extern "C"` inside `SWOS` namespace.

For example, following lines in input file:
```
@import
ExitEuropeanChampionshipMenu
GetFilenameAndExtension, int
```

Would produce the following in the resulting header:
```
extern "C" {
    namespace SWOS {
        extern void ExitEuropeanChampionshipMenu();
        extern int GetFilenameAndExtension();
    }
}
```

### `@export` section

`@export` section contains a list of symbols that are exported from the assembly and will be accessible from
C++ via the generated header file. Inside assembly files they will be prefixed with underscore, and made
public. Syntax for exporting symbols is:
```
<symbol> [, function |, (array | pointer | ptr | functionPointer) <type>['['<size>']'] |, <type>]
```

SWOS assembly file contains large amount of symbols, and limiting exports to only the used ones helps
improving compiling and linking performance, as well as minimizing name clashes.

Examples:

```
g_menuMusic, int16_t                            # becomes: int16_t g_menuMusic
skipIntro, word                                 # becomes: word skipIntro
dosMemLinAdr, char *                            # becomes: char *dosMemLinAdr
FadeOutToBlack, function                        # becomes: void FadeOutToBlack();
hilFileBuffer, array char                       # becomes: char hilFileBuffer[];
keyBuffer, array char[10]                       # becomes: char keyBuffer[10];
introTeamChantIndices, array const int8_t[16]   # becomes: const int8_t introTeamChantIndices[16];
sortedSprites, array Player * [79]              # becomes: Player *sortedSprites[79]
spriteGraphicsPtr, array SpriteGraphics *(*)    # becomes: SpriteGraphics *(*spriteGraphicsPtr)[];
```

### `@remove` section

`@remove` section contains a list of functions and variables that will be completely removed from the
resulting output files. It's commonly used to get rid of DOS-specific code, unused code/data and items
made obsolete by new portable C++ code.

To remove a range of symbols specify beginning and ending symbol separated by a `-` (minus) sign.
Ranges are semi-open, just like in C++ so listing

```
CDROM_GetInfo - Text2Sprite
```

will remove `CDROM_GetInfo` and everything following it up to but not including `Text2Sprite`.

Special symbol `@end` can be used as an end of range, specifying removal until the end of the section.

### `@null` section

Any procedure name listed in `@null` section will be turned into null proc, consisting of `retn` only.
It is recommended to remove unnecessary symbols, but sometimes it's not feasible as the symbol might be
referenced from many places in the code. In such cases large functions can be temporarily
nullified until all the code that references them can be removed.

### `@replace` section

This section enables replacing initial values of variables, as well as sizes of arrays. It will replace listed
variable content (after size specifier) with text specified after the comma. For example:

```
aQuitToDos, 'QUIT TO WINDOWS', 0
# original:   aQuitToDos db 'QUIT TO DOS',0
# changes to: aQuitToDos db 'QUIT TO WINDOWS', 0
g_soundOff, 0
# original:   g_soundOff dw 1
# changes to: g_soundOff dw 0
hilFilename, 256 dup(0)
# original:   hilFilename db 14 dup(0)
# changes to: hilFilename db 256 dup(0)
```

### `@save-regs` section

Since SWOS was written in assembly language it uses CPU registers freely, which can create problems when SWOS
procedures are called from C\++. Many functions destroy contents of registers C\++ compilers assume will not
change throughout a function call. In order to call SWOS code safely for each listed procedure instructions
will be injected that save and restore registers C\++ compiler considers nonvolatile.

Care has to be exercised as `push` and `pop` instructions are injected at the beginning of the proc, and
before the final `retn`, without code flow analysis. This can cause crashes if used with functions that
have multiple exit points. `SAFE_INVOOKE` macro can be used to call such functions.

This section is ignored when converting to C++ files.

### `@on-enter` section

For each procedure listed in this section a function call will be inserted as a first instruction. Name of this
function will be procedure name concatenated with `_OnEnter`. For example:
```
InitGame    # first instruction of InitGame will be call InitGame_OnEnter
```

On the C++ side parameterless `void` function inside a SWOS namespace will be declared in output header file,
`SWOS::InitGame_OnEnter()` for the example above.

### `@insert-hook` section

Lines in this section have the following form:
```
<proc name>, <line number>[, <hook name>]
```

Inside each procedure at a specified line number a call will be inserted to `<hook name>`, or to the
function in the form of `<proc name>_<line number>` if hook name wasn't given. For example:

```
GameLoop, 224
PlayerDoingHeader, 157, PlayHeaderComment
```

will cause a `call GameLoop_224` to be inserted at line 224 of `GameLoop` and
`call PlayHeaderComment` at line 157 of `PlayerDoingHeader` procedure. On the C++ side,
`SWOS::GameLoop_224()` and `SWOS::PlayHeaderComment()` will be declared in the output header file.

It is also possible to remove lines. To do so specify special hook name `@remove` at a line that needs to
be removed.

There are no limitations in number of hooks per procedure, as long as each line corresponds to exactly
one hook.

### `@type-sizes` section

This section is only valid for C++ output. In order to properly create structure containing all the SWOS
variables `ida2asm` must know the sizes of all types. Any type that can't be deduced from the input files
must be specified here.

This section is currently only used when outputting C++ files.

## `no-break` markers

`ida2asm` can be instructed to create multiple assembly files, breaking original file in several places in
the process. While care is taken that no procedures are broken apart, it can still happen with some data
tables or procedures that are expected to be consecutive. In such situations special markers in the source
code are introduced: "`; ${no-break`" begins protected block, "`; $no-break}`" ends it. Example usage:
```
; ${no-break
; this is important filenames table
db 'importantFileName1', 0
db 'importantFileName2', 0
db 'importantFileName3', 0
; $no-break}

; ${no-break
proc Proc1
    ; some logic
    ; no retn instruction, continue executing Proc2
endp
proc Proc2
    ; more logic
    retn
endp
; $no-break}

```

## 68k registers

Variables A0-A6 and D0-D7 are handled specially. They represent registers of virtual Motorola 68k CPU, and
are declared as instances of special templated class which offers a plethora of ways of accessing it
(as a byte, word, dword, pointer etc.). The variables will be automatically available in the generated
header file.

## C++ output

TODO:
- virtual machine
- memory allocation
- alignment
- `SwosDataPointer<>`
- `SwosProcPointer`

### Optimizations
TODO:
- redundant flag setting removal
- redundant assignment removal
- orphaned assignment removal

## Performance

First version of the `ida2asm` was written in Python, with the goal of getting the results as quick as
possible. This impacted performances negatively, resulting in processing times of 1 minute and upward. It was
extremely harmful for productivity as assembly files generation needed to be run fairly often, especially at
the beginning of the project.

That's how second parser version came to be, written in C\++ with ultimate performance in mind. This approach
is reflected in a specially constructed tokenizer (see next section), minimization of heap allocations,
cache-friendly structures and multi-threaded parser.

The end result was stunning: ~30ms on average to process 250k lines `swos.asm` on an i7-7700K :)

Many lessons were learned along the way. Importance of measuring can not be overstated. Low performance of
`std::unordered_map` and `std::isspace()` was surprising but undeniable.

### `gen-lookup`

`gen-lookup` is a Python script that will generate C++ function for tokenizing. It constructs it from input
file `tokens.lst` which contain informations about the instruction set. The generated function is a kind
of prefix tree encoded in the code (via switch or if statements). Example simplified pseudo-output would be:
```
// p -> input
...
if (*p++ == 'm')
  if (*p++ == 'o')
    if (*p++ == 'v')
      if (isDelimiter(*p)) return MOV
      else if (*p++ == 's')
        if (isDelimiter(*p)) return MOVS
        else if (*p++ == 'b')
          if (isDelimiter(*p)) return MOVSB
        else if (*p++ == 'd')
          if (isDelimiter(*p)) return MOVSD
        else if (*p++ == 'w')
          if (isDelimiter(*p)) return MOVSW
        else if (*p++ == 'x')
          if (isDelimiter(*p)) return MOVSX
...
```

It was selected by "winning" a performance war against a few alternatives:

* looking up tokens via `std::unordered_map`
* looking up tokens via custom hash map
* looking up tokens via `gperf` generated table and lookup function

State machine variants using ifs and switches were also tested. Project token-lookup-tester generates the
tester which was used to measure performance. Here's an output from one run on my machine:

```
Number of collisions for std::unordered_map: 65
Number of collisions for custom hash: 19

Time for state machine (switch): 73ms
Time for state machine (ifs): 79ms
Time for std::unordered_map: 145ms
Time for custom hash function: 128ms
Time for gperf generated lookup: 80ms
```

The interested ones can look further into token-lookup-tester project, and run the executable to see how
the performance results might look like on their machines.

### Parallel processing

Parsing of the input file and further processing and output is split and given to separate threads to process
it in parallel. Their number can be specified from the command line.

The input file can not be trivially split at random points. Each thread has to determine start and end of its
block, given only rough offset.

Since no locks or other synchronization primitives are used each thread is responsible of finding the safe
limits of its part. They all use the same algorithm of what to skip and where to begin/end and thus arrive
at mutually exclusive limits covering entire file.

First step is to ensure no lines are broken. Then the chunks are tokenized (with some additional buffer
before/after) and it is further assured that no procedure is broken or any `no-break` block.

Parsing begins after the hard block limits have been established.

### Data oriented programming
TODO

`ida2asm` uses mostly custom data structures, designed to maximize data locality. Any variable parts (e.g.
strings) follow immediately after the structure.

## Hacks and quirks

`ida2asm`, although fairly general tool, is quite tied to SWOS, and contains some peculiarities and
workarounds for weirdness in IDA syntax. Namely, support for virtual Amiga registers, as well as removal
of unaligned stack access and IDA never marking struct variables as such.

Still, adapting it for a novel purpose would likely be much easier than starting from scratch.

## Changing the output format to other assembler than MASM

Output is created by a class that inherits abstract base class `OutputWriter`. `OutputWriter` handles
writing to the actual output file and exposes set of output functions for the inherited classes to use.
Descendant class will be created by the factory and given access to all the stored data from parsing.
Its task is to use this data and the given interface to generate a valid assembly file.

`VerbatimOutput` is an example output class that will output all the input "as is". It's a very useful
tool for testing and can be a good place to start when trying to figure out how things work.

For portability do not output new line directly. Use `out(Util::kNewLine)` instead. Use `out(kIndent)` to
start an indented line. Base class will handle the rest, as well as tabs.
