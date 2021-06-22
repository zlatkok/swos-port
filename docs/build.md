# Requirements

Currently, the game only builds and runs on Windows and Android. To build you must have the following
installed and available in the system path:

[Windows]
- Visual Studio 2019 for the tests, 2015 for the rest (maybe older would work too)
- Python 3.6+
  - ddt (for mnu2h tests)
  - PyTexturePacker, for building sprite maps
- MASM at least version 8
- gperf (optional, for tokenizer performance test)
- Cygwin (optional, for backup script)
<!-- - Inno Setup (for the installer) [later] -->

Required libraries (as *.lib files in lib dir):
- SDL2
- SDL2_image (currently only PNG's used, for screenshots)
- SDL2_net \[currently unused\]
- SDL_Mixer
- ADLMIDI (to play menu.xmi)
- CrashRpt (crash reporter)

Include only libraries:
- SimpleIni


[Android]
- Android Studio
- TODO...


Also corresponding dll's are required to run.

For non-Windows systems Meson build scripts are under development.

# Build process
<!-- TODO, describe required defined symbols (DEBUG,NDEBUG,SWOS_TEST) etc.-->

`swos-port` is composed of several inter-dependent projects. Goal of this document is to describe those
dependencies and their build steps in high-level and give overview to help anyone interested in understanding
the build system, improving it or creating a better one. However as mentioned this is only a high level
overview, for complete details Visual Studio/Meson build files would need to be consulted.

## `ida2asm`

Project at the bottom of dependency chain that builds first is`ida2asm`. Detailed informations about `ida2asm` can
be found [here](ida2asm.md), but in a nutshell it is a tool to convert assembly output from IDA into a real assembly
file(s) that can be assembled by MASM cleanly without errors.

First step in building `ida2asm` is to build the token lookup function. It is generated from Python source
file: `ida2asm/gen-lookup/gen-lookup.py`. The Python script depends on token list `ida2asm/gen-lookup/tokens.lst`
and will output `TokenTypeEnum.h` and `TokenLookup.h`. Those will contain lookup function used by the
tokenizer which is a state machine built to recognize tokens from `tokens.lst`.

When successfully built these files are included by the rest of C++ sources located in `ida2asm/src`,
which are then compiled in the usual manner.

## `token-lookup-tester`

It depends on the same two files as `ida2asm`'s token lookup:

- `ida2asm/gen-lookup/gen-lookup.py`
- `ida2asm/gen-lookup/tokens.lst`

It will run the script with different parameters and end up with the main output of `main.cpp`.
`main.cpp` is then compiled into tester executable in the usual way. Tester is capable of running tests for
the generated token lookup function, as well as performance tests comparing it to several alternative
implementations. Optional dependency is `gperf`, which is looked up during the build, and used in performance
tests if found.

Note that the tester is only built, but not executed in any way.

## `swos-asm`

This intermediate project invokes `ida2asm` binaries on `swos/swos.asm` (SWOS disassembly) and
`swos/symbols.txt` (control file for `ida2asm`, for more details see [`ida2asm`](ida2asm.md)). The result
are actual ASM files which will be fed to MASM, as well as `swossym.h`, a C++ header file containing exported
ASM symbols that need to be accessed from C++. To speed up the build process 8 assembly files are generated,
`swos-01.asm` to `swos-08.asm`.

One problem of the current build is that debug version will try to use release version of `ida2asm`, since
it runs much faster. I was unable to find a way to make Visual Studio aware of this, so unfortunately debug
build will require running release at least once.

## `swos-c`

This is an experimental project converting `swos.asm` into C files rather than MASM assembly, in goal of
being able to compile for pretty much any platform. Outputs will be C files, `swos-01.c` to `swos-08.c`.
Additional outputs: `vm.c`, `vm.h`, `defs.h`, `cTables.h`.

Outputs are subject to change as the project isn't stable yet.

### `mnu2h`

`mnu2h` is a small SWOS menu compiler written in Python. More details can be found [here](mnu2h.md). Source
files are located in `mnu2h` directory. It will convert a menu file, say `example.mnu`, into a header file
`example.mnu.h` which can be included and used from C++ code. Currently each *.mnu file is a part of the
main project and builds as a custom tool.

Tests are located under `mnu2h/tests` and are not currently part of the build.

## `swos-port`

This is the main project, on top of the dependency chain. Source files are located in `src/`. It will combine
SWOS assembly files, encoded SWOS menus from `mnu2h` with C++ files and libraries to build a final executable
of the game.

Generated assembly files get assembled with MASM in this phase.

Visual Studio project files are in `vc-proj` directory, and the main solution file for the project is
`swos-port.sln`.

## `sdl-address-table-fetcher`

This is a prerequisite for tests, it builds a small DLL which will gain access to SDL's dynamic table of
functions in order to allow mocking them.

There is nothing unusual about the build.

## `tests`

This project contains simple testing framework and unit tests for `swos-port`. It is made dependant of main
project because it requires the same compiled menus. Repeating all the custom build commands in this project
would result in significant duplication. This is another problem of the current build.

Tests are only built, but not ran.
