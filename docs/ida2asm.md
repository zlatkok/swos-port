# `ida2asm`

`ida2asm` is a tool for converting pseudo-assembly output from IDA Pro into something that can actually be
assembled/compiled without errors.

Even though it started as an assembler conversion tool it can now generate C++ output in addition to the
basic MASM x86 format which has even become obsolete. Having multiple output formats was an idea from the
start so it was designed to be easily augmented.

Aside from the conversion, second task is symbol manipulation (importing, exporting, removing, etc.). This
is specified via a custom format input text file.

## Command-line parameters

`ida2asm` expects 6 command line parameters, in the following order:
- path to the exported input ASM file from IDA
- directory path for output files (it will be extracted if a file path is given)
- path to input control file
- path to output C++ header with exported symbols
- output format (currently only `masm` and `c++` are supported)
- number of files to split output files (as well as how many threads to use), maximum is 20

Optional parameters are also accepted, they start with two dashes, and are currently only valid for C++
output:

`--extra-memory-size=<size>` - statically allocate memory of given size

`--disable-optimizations` - disable conversion-time optimizations

`--disable-alignment-checks` - generate direct word and dword memory accesses instead of the portable
unaligned access helpers. This is intended for targets where unaligned access is supported and should only
be used when the resulting platform assumptions are acceptable.

`--report-unreferenced` - print procedures and named data retained in the VM image but not reachable from
an exported symbol or a top-level reference.

`--unreferenced-report=<path>` - write the same report to a file instead of standard output.

Short help can be listed by passing `-h` or `--help` parameter. See more about optimizations in
[Optimizations section](#optimizations).

### Unreferenced VM item report

The report follows direct symbolic references in instructions and initialized data (including pointer
tables). Exported symbols and references outside a procedure or named data item are treated as roots. Each
unreachable item is listed as a procedure or data item with its source line. Consecutive unreachable items
of the same kind are additionally folded into semi-open `first - end` regions compatible with `@remove`;
`end` is the first item not contained in the region. Every individual contained item remains listed under
the region heading.

This is deliberately a report only: it does not remove anything. Computed or otherwise indirect references
that do not name their target cannot be inferred, so every reported item must be reviewed before adding it
to `@remove`. The reachability graph is built only when one of the reporting options is present; ordinary
swos-port generation retains its existing conversion path apart from a single option check.

## Input file

Input file controls how the conversion to the actual output files will be performed. It consists of
sections which begin with '`@`' sign. Following sections are supported:
- `@import`
- `@export`
- `@remove`
- `@null`
- `@replace`
- `@save-regs`
- `@on-enter`
- `@insert-hook`
- `@constant-to-variable`
- `@type-sizes`

Any line beginning with `#` sign is considered a comment and will not be processed.

### `@import` section

The purpose of `@import` section is to replace assembly procedures with functions from C++. Any non-empty
line is treated as a procedure name that will be imported from C++. C++ functions do not take any
parameters and by default return `void`, unless symbol name is followed by a comma followed by `int` -- in
that case return type is `int`. Assembly procedure body is removed from generated assembly code, and its
name is treated as an extern label. All the imported C++ functions are listed in the generated header file,
declared as `extern "C"` inside `SWOS` namespace.

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

`@export` section contains a list of symbols that are exported from the assembly and will be accessible
from C++ via the generated header file. Inside assembly files they will be prefixed with underscore, and
made public. Syntax for exporting symbols is:
```
<symbol> [, function |, (array | pointer | ptr | functionPointer) <type>['['<size>']'] |, <type>] [(align: <n>)]
```

SWOS assembly file contains a large amount of symbols, and limiting exports to only the used ones helps
improving compiling and linking performance, as well as minimizing name clashes.

Examples:

```
g_menuMusic, int16_t                            # becomes: int16_t g_menuMusic
skipIntro, word                                 # becomes: word skipIntro
dosMemLinAdr, char *                            # becomes: char *dosMemLinAdr
FadeOutToBlack, function                        # becomes: void FadeOutToBlack();
hilFileBuffer, array char                       # becomes: char hilFileBuffer[1];
keyBuffer, array char[10]                       # becomes: char keyBuffer[10];
introTeamChantIndices, array const int8_t[16]   # becomes: const int8_t introTeamChantIndices[16];
sortedSprites, array Player * [79]              # becomes: Player *sortedSprites[79]
spriteGraphicsPtr, array SpriteGraphics *(*)    # becomes: SpriteGraphics *(*spriteGraphicsPtr)[1];
g_currentMenu, array char[6500] (align: 4)      # becomes: char g_currentMenu[6500]; (with 4 byte alignment)
```

`align` modifier can be used to manually specify the alignment of the variable in bytes.

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

Special symbol `@end` can be used as an end of range, specifying removal until the end of the assembly input.
This also applies when the range spans multiple parallel parsing chunks.

ida2asm warns when two `@remove` entries meet at the same boundary and can be consolidated, such as
`A - B` with either `B - C` or a standalone `B`. The check is performed once while parsing the symbols
file and does not scan the assembly input.

### `@null` section

Any procedure name listed in `@null` section will be turned into null proc, consisting of `retn` only.
It is recommended to remove unnecessary symbols, but sometimes it's not feasible as the symbol might be
referenced from many places in the code. In such cases large functions can be temporarily nullified until
all the code that references them is removed.

### `@replace` section

This section enables replacing initial values of variables, as well as sizes of arrays. It will replace
listed variable content (after size specifier) with text specified after the comma. For example:

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

Since SWOS was written in assembly language it uses CPU registers freely, which can create problems when
SWOS procedures are called from C++. Many functions destroy contents of registers C++ compilers assume will
not change throughout a function call. In order to call SWOS code safely for each listed procedure
instructions will be injected that save and restore registers C++ compiler considers nonvolatile.

Care has to be exercised as `push` and `pop` instructions are injected at the beginning of the proc, and
before the final `retn`, without code flow analysis. This can cause crashes if used with functions that
have multiple exit points. `SAFE_INVOOKE` macro can be used to call such functions.

This section is ignored when converting to C++ files.

### `@on-enter` section

For each procedure listed in this section a function call will be inserted as a first instruction. Name of
this function will be procedure name concatenated with `_OnEnter`. For example:
```
InitGame    # first instruction of InitGame will be call InitGame_OnEnter
```

On the C++ side parameterless `void` function inside a SWOS namespace will be declared in output header
file, `SWOS::InitGame_OnEnter()` for the example above.

### `@insert-hook` section

Lines in this section have the following form:
```
<proc name>, <line number>[, <hook name>]
<proc name>, <label>, <hook name>
```

Inside each procedure at a specified line number a call will be inserted to `<hook name>`, or to the
function in the form of `<proc name>_<line number>` if hook name wasn't given. For example:

```
GameLoop, 224
PlayerDoingHeader, 157, PlayHeaderComment
```

will cause a `call GameLoop_224` to be inserted at line 224 of `GameLoop` and `call PlayHeaderComment` at
line 157 of `PlayerDoingHeader` procedure. Previous content that comes after inserted lines is shifted one
line above, e.g. old lines 224 and 157 become new lines 225 and 158, respectively. On the C++ side,
`SWOS::GameLoop_224()` and `SWOS::PlayHeaderComment()` will be  declared in the output header file.

A procedure-scoped label can be used instead of a line number. The call is inserted at the label entry,
after the label itself and before its first instruction. Unlike line numbers, label targets are unaffected
by blank lines, comments, or formatting changes. For example:

```
AdjustPlayerSkills, @@set_goalie_skill, FixTwoCPUsGameCrash
```

inserts `call FixTwoCPUsGameCrash` immediately after the local `@@set_goalie_skill` label in
`AdjustPlayerSkills`. Label-based hooks require an explicit hook name. If the label is not found in the
specified procedure, conversion fails and reports both the missing label and procedure. The `@remove`
special hook is only supported with numeric line targets.

It is also possible to remove lines. To do so specify special hook name `@remove` at a line that needs to
be removed.

There are no limitations in number of hooks per procedure, as long as each line corresponds to exactly
one hook.

### `@constant-to-variable` section

Sometimes it is required to use a variable instead of a constant. That can be achieved by adding a line in
this section with the following format:

```
<proc name>, <line number>, <variable name>, <constant value>
```

Procedure name and line number uniquely identify a line with constant access. Line number is counted from
the beginning of the procedure, starting with 1. If the specified line does not hold an instruction using a
constant value, or a constant value does not match a specified value, an error will be reported. Supplied
constant value isn't strictly necessary, but it helps validating it's the right line.

`ida2asm` will replace the constant with a variable when generating output. Given variable will be added to
the end of SWOS data memory and initialized with original constant value. The variable will appear as any
other SWOS variable, and can be accessed with `swos.<variable name>` syntax. Variable name is expected to
be valid and unique in this context.

For example:
```
UpdateCameraBreakMode, 8, penaltiesInterval, 110
```
will generate instruction:
```
cmp penaltiesTimer, penaltiesInterval
```
in place of:
```
cmp penaltiesTimer, 110
```
and a variable `penaltiesInterval` will be added to SWOS variables and initialized to 110.

Note that this is technically invalid code in x86 assembly, since there is no addressing mode capable of
moving data between two memory locations. However the code generator will handle it properly and generate
correct assignment.

### `@type-sizes` section

This section is only valid for C++ output. In order to properly create structure containing all the SWOS
variables `ida2asm` must know the sizes of all types. Any type that can't be deduced from the input files
must be specified here.

This section is currently only used when outputting C++ files.

## `no-break` markers

`ida2asm` can be instructed to create multiple output files, breaking original file in several places in
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

## Output

### Verbatim

Verbatim output simply outputs gathered input as is, without any additional processing. Its primary use is
for testing, and as a starting point when implementing new output formats.

### MASM output

_**MASM output has been deprecated. It is still present, but lacks any new features.**_

By using MASM output format the pseudo-assembly input is converted to proper MASM 32-bit syntax that can be
assembled with MASM version 8.00.50727.104. Older versions would probably work too, but this one has been
extensively tested. Limitations are that only the systems running on 32-bit Intel processors are supported,
and there are no optimizations on the output files -- it is practically a one-to-one conversion of the
input instructions.

### C++ output

C++ output is the main output format of the application. The gathered input (x86 instructions) is
translated to a compilable C++ code, backed by the SWOS Virtual Machine™ runtime. The code is highly
portable and can run on virtually any system supported by a C++ compiler. Generated output files are:

- `swossym.h`
- `vm.cpp`
- `vm.h`
- `swos*.cpp` (e.g. `swos-01.cpp`, `swos-02.cpp`...)

`swossym.h` is a support header containing SWOS namespace, `Register` struct definition used for 68k
register set, function prototypes (for both SWOS code -> external code and external code -> SWOS code),
`ida2asm`-introduced variables and some compile-time checks.

`vm.cpp` and `vm.h` contain implementation of the SWOS Virtual Machine™, which will be further explained.
`swos*.cpp` files contain the actual SWOS binary code, converted to C++ that runs in the SWOS Virtual
Machine™. The number of files is decided by the command line parameter.

#### SWOS Virtual Machine™

SWOS Virtual Machine™ is a static (compile-time) emulator of Intel x86 instructions, and is the target for
`ida2asm` C++ output. It supplies the Intel 80386 register set and flags as global variables and a SWOS
memory address space where all the game's data resides. There is also a small runtime component that
manages memory access, ensures proper alignment and supports inter-operation with the external code,
residing in `vm.cpp`.

Each instruction is translated when the input file is processed and converted to one or more C++
statements. CPU register access is converted to global variable access, and memory access to VM data area
access. Generated statements update the virtual CPU state in the same way as the original instructions.

Since it's a compile-time emulator custom tailored to SWOS, and implements only a minimal subset of x86
instruction set required to run the game, it is able to achieve much higher performance than a general
purpose dynamic emulator would. Also, it's able to exploit very good optimization capabilities of modern
C++ compilers that will compile the generated code, although it does implement a few optimizations of its
own. Thus the generated code has excellent performance, very close to native.

The generated interface keeps game declarations and VM implementation details separate. Game types and
exported symbols remain in their normal project headers, while registers, flags, memory, procedure dispatch
and generated variables live in the `SwosVM` namespace. The `swos` macro provides convenient access to the
generated `SwosVariables` instance. VM data and procedure pointers remain 32-bit values through
`SwosDataPointer<T>` and `SwosProcPointer`, even when the host program itself uses 64-bit pointers. These
pieces are described below.

#### Memory layout

Entire SWOS data area is laid out in an array linearly, thus resolving problems with non-consecutive
locations in assembly version -- when segmented assembly files were linked, linker would realign them and
sometimes these alignment bytes would fall inside SWOS tables and areas that are expected to be contiguous.

Memory space is divided into 4 parts. First part contains variables from the game -- the game's data area.
Second part holds the extended memory the game allocates from the DOS extender. Third part is so called
"pointer pool area", an array of native pointers and the way native memory can be accessed from within the
game. Fourth part holds memory which can be allocated to clients outside the virtual machine.

Each part is separated by a "safe area", filled with a fixed byte pattern. Checking if this pattern is
intact can discover memory overwrites. It's a curiosity that the first safe area (at offset 0) had to be
modified to have the first four bytes zeroed out, to emulate DOS/Amiga behavior since there are some null
pointer accesses in the original game (or perhaps the conversion, but it's not clear) that rely on getting
zero value to work properly.

Memory layout diagram:
```
  +---------------+
  |   safe area   |
  +---------------+
  |   basic mem   |    (all the variables)
  +---------------+
  |   safe area   |
  +---------------+
  |  extended mem |    (memory the game allocates from the OS)
  +---------------+
  |   safe area   |
  +---------------+
  | pointer pool  |    (native pointers, pointing outside the SWOS VM)
  |     area      |
  +---------------+
  |   safe area   |
  +---------------+
  |  dynamic mem  |    (memory allocated by the code outside the SWOS VM)
  |  (allocated)  |
  +---------------+
  |   safe area   |
  +---------------+
```

#### Access by external code

SWOS variables are grouped inside one big C++ `struct SwosVariables`. It maps one-to-one to location of
each variable when overlayed on top of SWOS memory data array. For convenience `swos.<variable>` macro is
provided, so for example it's enough to write:
```
swos.chosenTactics = -1;
```

to read/write variables. Types are preserved, so arrays or structs can be used freely:
```
swos.importTacticsFilename[0] = 0;
const auto& positions = swos.positionsTable[tactics];
if (swos.ballSprite.y >= kPitchCenterY) ...
if (swos.topTeamData.playerNumber != m_playerNumber) ...
```

Registers are instances of the generated `Register` class, which has some special sauce. A register stores
one 32-bit value, exposes its low word and low/high bytes, accepts integer values and `SwosDataPointer<T>`
objects, and converts VM offsets back to host pointers when requested. This makes both generated code and
replacement C++ code resemble the original assembly without exposing the address translation at every
access:
```
SwosVM::eax = &swos.ballSprite;                // host pointer converted to a VM offset
auto sprite = SwosVM::esi.as<Sprite *>();      // VM offset converted to a host pointer
word value = SwosVM::eax.asWord();             // low 16 bits
SwosVM::eax += sizeof(Sprite);                 // arithmetic is performed on the stored 32-bit value
```

Registers and flags are inside the `SwosVM` namespace and can be accessed as normal variables:
```
SwosVM::eax++;
SwosVM::ax = swos.gameCanceled;
SwosVM::flags.zero = !SwosVM::ax;
```

`SwosVM::flags` contains the four flags used by the converted code: carry, zero, sign and overflow.

#### Alignment

One thing external code has to look out for is the data alignment. SWOS very often accesses unaligned data
since the Intel processor allows that. To be portable across platforms that don't support unaligned access,
or have high performance penalty, care must be taken.

Generated structures are packed to preserve the original byte layout. Consequently, a `SwosDataPointer<T>`
field may itself be unaligned. Its normal conversion and dereference operators check that the stored 32-bit
offset is aligned; use `asAligned()`, `asAlignedDword()`, `fetch()`, `loadFrom()`, `set()` or `setRaw()`
when the pointer field must be loaded or stored through the alignment-safe helpers. For an aligned pointer
object, `getRaw()` returns the untranslated 32-bit VM value and is useful when passing an address back to
VM code.

`SwosDataPointer<T>` performs pointer arithmetic in VM address space and translates through
`SwosVM::offsetToPtr()` only when host code dereferences it. It must remain exactly four bytes wide so
generated structures retain their original layout.

Code generated inside the SWOS Virtual Machine is safe by default: `readMemory()` and `writeMemory()` split
unaligned word and dword operations into valid accesses. The `--disable-alignment-checks` option replaces
this portable behavior with direct accesses and should only be enabled for a suitable target.

#### Pointer pool area

Sometimes there's a need to pass outside data to a code running inside the virtual machine, or execute some
new non-game code inside it that needs outside memory access. SWOS is a 32-bit program using 32-bit
pointers, so on a 32-bit system native and VM pointers are interchangeable and can be passed around freely,
but running on a 64-bit system poses a challenge.

To address this, pointer pool was introduced. It is a list of native pointers that can be used by the VM
code. Special 32-bit VM pointers are created that are actually indices into the pointer pool with the
highest bit set. Virtual machine detects these special pointers when trying to access the memory, and
translates them appropriately. This way code inside the VM is allowed to break free and access host memory.

Code that needs to access native memory doesn't need to know how this mechanism works. There is an
interface function to register a native pointer:
```
dword SwosVM::registerPointer(const void *ptr);
```
It will register a native pointer `ptr` inside the pool pointer area, and return a 32-bit pointer valid
inside the virtual machine. Virtual machine will dereference a given native pointer each time a returned VM
pointer is accessed. In case of an error the function returns -1: if `ptr` is null or if the pointer pool
is full.

One example of usage is in menu system: string tables contain a pointer to an index of the currently
selected string, which gets updated by the user actions. Without pool pointers it would be very difficult
to create any new string table and seamlessly support this functionality.

It could also be used if some game buffers accessed via pointers would need to be extended: all it would
take would be to initialize those pointers with registered pool pointers tied to larger buffers outside the
VM.

#### Function pointers

Assembly procedure addresses cannot be represented as ordinary data pointers in generated C++. Instead,
`SwosProcPointer` stores a 32-bit index into the VM procedure table. Calling the object dispatches through
`SwosVM::invokeProc()`, while `empty()` and its boolean conversion test whether the index resolves to a
procedure. This preserves four-byte structure layouts and supports procedure pointers stored in game data.

Procedures generated from the original game have stable entries in the table. Host functions can be added
at runtime with `SwosVM::registerProc()`, which is useful for callbacks such as menu handlers.
`getProcMark()` and `releaseProcs()` provide stack-like lifetime management for these additional entries.

#### VM services

The generated `vm.h` also declares the runtime interface used by generated and external code:

- `readMemory()` and `writeMemory()` access one, two or four bytes through a 32-bit VM address and handle
pointer pool addresses and, by default, unaligned accesses.
- `g_memWord` and `g_memDword` provide typed views of VM memory. They are primarily intended for generated
code; external code should prefer `swos`, `SwosDataPointer<T>` or the memory access functions as
appropriate.
- `allocateMemory()`, `allocateMemoryTyped<T>()` and `allocateString()` allocate from the dynamic VM area
using a four-byte-aligned bump allocator. `cacheString()` reuses allocations for the same host string
pointer.
- `getMemoryMark()`/`releaseMemory()`, `getPointerPoolMark()`/`releasePointers()` and
  `getProcMark()`/`releaseProcs()` restore their respective pools to an earlier mark. `markAllMemory()` and
  `releaseAllMemory()` capture and restore all of them, including the string cache, together.
- `getExtraMemoryArea()` returns the extended-memory area whose size is selected with
`--extra-memory-size`.
- Debug builds can initialize and verify the safe areas or dump VM memory to a file.

#### Optimizations

Currently, the C++ generator performs three types of optimizations:

- **Redundant flag setting removal** suppresses overflow calculations in procedures that never test the
overflow flag. Other flags may be observable as procedure return values and therefore require more
conservative handling.
- **Redundant assignment removal** tracks known register and VM-memory contents and removes instructions
that merely reload a value already present at the destination.
- **Orphaned assignment removal** follows dependency chains through registers and known memory locations
and removes assignments whose results are overwritten without being observed.

Analysis is performed on ida2asm's intermediate instruction form and is deliberately conservative around
labels, branches, calls and unknown memory effects. Besides reducing runtime work, removing redundant
generated statements reduces the amount of C++ that the host compiler must parse and optimize. This is
particularly useful because IDA output contains many register reloads and other assembly-level operations
that become redundant after conversion to C++.

In case the optimizations are undesired, they can be disabled by supplying command line parameter
`--disable-optimizations`.

## Performance

First version of the `ida2asm` was written in Python, with the goal of getting the results as quick as
possible. This impacted performances negatively, resulting in processing times of 1 minute and upward. It
was extremely harmful for productivity as assembly files generation needed to be run fairly often,
especially at the beginning of the project.

That's how second parser version came to be, written in C++ with ultimate performance in mind. This
approach is reflected in a specially constructed tokenizer (see next section), minimization of heap
allocations, cache-friendly structures and multi-threaded parser.

The end result was stunning: ~30ms on average to process 250k lines `swos.asm` on an i7-7700K :)

Many lessons were learned along the way. Importance of measuring can not be overstated. Low performance of
`std::unordered_map` and `std::isspace()` was surprising but undeniable.

### `gen-lookup`

`gen-lookup` is a Python script that will generate C++ function for tokenizing. It constructs it from input
file `tokens.lst` which contain informations about the instruction set. The generated function is a kind
of prefix tree encoded in the code (via switch or if statements). Example simplified pseudo-output would
be:
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

It was selected by winning a "performance war" against a few alternatives:

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

Parsing of the input file and further processing and output is split and given to separate threads to
process in parallel. Their number can be specified from the command line.

The input file can not be trivially split at random points. Each thread has to determine start and end of
its block, given only rough offset.

Since no locks or other synchronization primitives are used each thread is responsible of finding the safe
limits of its part. They all use the same algorithm of what to skip and where to begin/end and thus arrive
at mutually exclusive limits covering entire file.

First step is to ensure no lines are broken. Then the chunks are tokenized (with some additional buffer
before/after) and it is further assured that no procedure is broken or any `no-break` block.

Parsing begins after the hard block limits have been established.

### Data oriented programming

`ida2asm` uses mostly custom data structures, designed to maximize data locality. Any variable parts (e.g.
strings) follow immediately after the structure.

The tokenizer is the clearest example: tokens and their text are stored consecutively in a single arena,
and a token locates the next token by advancing over its fixed header and trailing text. Parsing can
therefore walk the stream without allocating an object or following a separately allocated string for every
token. Parsed output items use a similar packed representation for comments and item-specific data.

Collections that grow during conversion are generally reserved using capacities derived from the SWOS
input, and several lookup structures are built once and then sealed before concurrent read-only processing.
This avoids allocator traffic in hot loops, improves cache locality and makes it possible for workers to
operate without locks. The trade-off is that some capacities and representations are tailored to the
expected input and guarded by assertions or explicit limits rather than behaving like fully general-purpose
containers.

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
