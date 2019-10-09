# About

Welcome to the unofficial port of Sensible World of Soccer 96/97.
Sensible World of Soccer, or SWOS 96/97 is a soccer game developed by Sensible Software.

This is the port of the PC DOS version. Currently the only supported platform is Windows but
it is developed using C++ and [SDL](https://www.libsdl.org) and there are plans for ports to
other platforms in the future.

Game style is DOS, with Amiga style currently in the works.

## Building

For more information about building see [build.md](docs/build.md).
<!-- TODO, describe required defined symbols (DEBUG,NDEBUG,SWOS_TEST) etc.-->

## Tools

There are a couple of tools that are used heavily during the development.

### ida2asm

`ida2asm` is a tool for converting pseudo-assembly output from IDA Pro into
something that can actually be assembled without errors. For more information
please see [ida2asm.md](docs/ida2asm.md).

### mnu2h

mnu2h is a mini-compiler for menu (*.mnu) files. It is converting them into C++
header files which compile directly to SWOS binary menu format. For more information
please see [mnu2h.md](docs/mnu2h.md).

## tests

For more information about tests please see [tests.md](docs/tests.md).
