# Build process

`swos-port` is composed of several inter-dependent projects. Goal of this document is to describe those
dependencies and their build steps in high-level and give overview to help anyone interested in understanding the
build system, improving it or creating a better one.

## `ida2asm`

Project at the bottom of dependency chain that build first is`ida2asm`. Detailed informations about `ida2asm` can
be found [here](ida2asm.md), but in a nutshell it is a tool to convert assembly output from IDA into a real assembly
file(s) that can be assembled by MASM cleanly without errors.

In order for it to build successfully token lookup function must be generated from Python source file:
`ida2asm/gen-lookup/gen-lookup.py`. The Python script depends on token list `ida2asm/gen-lookup/tokens.lst`
and will output `TokenTypeEnum.h` and `TokenLookup.h`.

When successfully built they are included by the rest of C++ sources located in `ida2asm/src`, which are then
compiled in the usual manner.

## `token-lookup-tester`

It depends on same files as `ida2asm`:

- `ida2asm/gen-lookup/gen-lookup.py`
- `ida2asm/gen-lookup/tokens.lst`

but will run script with different parameters and its output will be `main.cpp`. `main.cpp` is then compiled into
tester executable the usual way. Tester is capable of running tests for the generated token lookup function, as well
as performance tests comparing it to several alternative implementations. Optional dependence is `gperf`, which is
looked up during the build, and used in performance tests if found.

Note that the tester is only built, but not executed in any way.

## `swos-asm`

This intermediate project invokes `ida2asm` binaries on `swos/swos.asm` (SWOS disassembly) and `swos/symbols.txt`
(control file for `ida2asm`, for more details see [`ida2asm`](ida2asm.md)). The result are actual ASM
files which will be fed to MASM. To speed up the build process 8 ASM files are generated, `swos-01.asm` to
`swos-08.asm`.

One problem of the current build is that debug version will try to use release version of `ida2asm`, since it runs
much faster. I was unable to find a way to make Visual Studio aware of this, so unfortunately debug build will
require running release at least once.

### `mnu2h`

`mnu2h` is a small SWOS menu compiler written in Python. More details can be found [here](mnu2h.md). Source files
are located in `mnu2h` directory. It will convert a menu file, say `example.mnu`, into a header file
`example.mnu.h` which can be included and used from C++ code. Currently each *.mnu file is a part of the main
project and builds as a custom tool.

Tests are located under `mnu2h/tests` and are not currently part of the build.

## `swos-port`

This is the main project, on top of the dependency chain. Source files are located in `src/`. It will combine SWOS
assembly files, encoded SWOS menus from `mnu2h` with C++ files and libraries to build a final executable of the
game.

## `sdl-address-table-fetcher`

This is a prerequisite for tests, it builds a small DLL which will gain access to SDL's dynamic table of functions
in order to allow mocking them.

There is nothing unusual about the build.

## `tests`

This project contains simple testing framework and unit tests for `swos-port`. It is made dependant of main project
because it requires same compiled menus. Repeating all the custom build commands in this project would result in
significant duplication. This is another problem of the current build.

Tests are only built, but not ran.
