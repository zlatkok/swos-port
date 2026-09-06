# Tests

The repository has four separate test suites:

- the C++ SWOS test harness in `tests/`, which covers menus, input devices, commentary and recorded games;
- the generated C++ token lookup tester for `ida2asm`;
- the Python unit tests for the `mnu2h` menu compiler in `mnu2h/tests/`;
- the Python unit tests for the animation-table generator in
  `src/game/animation/testGenerateAnimationTables.py`.

The C++ tests use the project's small in-house framework rather than an external test framework. They run
against mocked SDL, rendering, audio, file-system and timing functions, so most tests do not open a game
window or run in real time.

## Building and running the C++ tests

On Windows, open `project/vc++/swos-port.sln` in Visual Studio and build the `tests` project in the desired
Debug or Release, x86 or x64 configuration. The solution also builds `SdlAddressTableFetcher.dll`, which
the test executable needs,
and the project copies that DLL beside the executable. Build output is placed under
`tmp/tests-<platform>-<configuration>/`; for example, an x64 Debug build produces:

```powershell
.\tmp\tests-x64-Debug\testsDebug.exe
```

Running the executable without arguments runs every C++ test. It returns zero when all tests pass and a
non-zero exit code when a test fails. Run it with `--help` (or `/?`) to see the current options:

```text
testsDebug.exe [options] [test name ...]

--swos-dir=<path>       set the directory containing SWOS data
--create-snapshots      save rendered menu snapshots as BMP files under snapshots/
--pass-on-exceptions    let exceptions reach an attached debugger
--exit-on-first-fail    stop after the first failure
--timeout=<integer>     set the requested test timeout
```

A test-suite name, case ID or case display name may be supplied to run only matching tests. Multiple names
may be supplied. An unknown name is reported as an error. The output prints a dot for a passing case, `F`
for an assertion failure and `E` for an unexpected exception, followed by a summary.

Test suites normally run in their static registration order, which is not guaranteed across translation
units. Suites marked by the harness as last-running are moved to the end while preserving the relative
order of all other suites. `RecordedDataTest` is marked this way, and its recorded-match simulation is its
last case, so the simulation runs last when the complete test set is selected.

## Semi-automatic menu snapshot checks

The C++ harness can also exercise the menu tests and capture the rendered result of each tested menu state:

```powershell
cd .\tmp\tests-x64-Debug
.\testsDebug.exe --create-snapshots
```

The tests still perform their normal assertions, while `--create-snapshots` adds a BMP file after each test
case that drew a menu and permits snapshots. Data-driven cases produce one image per input state. Files are
written to `snapshots/` below the process's current working directory, so running as shown above places them
in `tmp/tests-x64-Debug/snapshots/`.

Snapshot names consist of the stable test-case ID and a zero-padded data index, for example
`custom-win-size-003.bmp`. Re-running the same cases overwrites files with matching names, but the harness
does not remove obsolete files left by older or broader runs. Empty or move aside the snapshot directory
before a complete review so stale images cannot be mistaken for current output.

This is deliberately a semi-automatic visual check: the harness creates the images but does not compare
them with golden files. A reviewer must browse the resulting BMPs and look for clipping, overlaps, wrong
text or colors, bad selection states, missing sprites and incorrect layouts or resolutions. The process exit
status only reports programmatic assertions and does not indicate that the images were reviewed or visually
correct. Recorded-game and other non-visual cases explicitly disable snapshot capture.

Even with manual review, this is much faster and more repeatable than navigating through every menu in the
game. The tests automatically reach deeply nested or unusual states and construct fixtures that would be
tedious to reproduce by hand—for example, populating a data directory with many files having deliberately
long or otherwise specific names to exercise scrolling, sorting and text elision. Consequently, snapshot
review should be preferred over ad-hoc manual menu traversal when checking broad UI changes.

Meson builds the same test executable as `tests/tests.exe` below its selected build directory. Invoke it
directly and pass `--swos-dir=<path>` when running suites that need original SWOS data. It is deliberately not
registered as an argument-free `meson test` case because the required SWOS installation path is machine-specific.

## Mocking SDL functions

Testing the game requires control over calls to SDL and the ability to reroute them to deterministic mock
implementations. SDL keeps an internal table of pointers to its public API functions. Its normal purpose is
to allow the SDL implementation to be overridden, for example by Valve's SDL or by a newer SDL build when
the library was linked statically.

SDL checks the `SDL_DYNAMIC_API` environment variable at startup and treats its value as the path to an
external DLL. It loads the DLL and calls:

```cpp
Sint32 SDL_DYNAPI_entry(Uint32 version, void *table, Uint32 tablesize);
```

The test solution builds `tests/sdl-address-table-fetcher`, whose entry point records the table address and
size, then returns `-1`. SDL consequently fills the table with its own valid function pointers. The test
harness can later retrieve the recorded table and replace selected entries with mocks. This setup is done
automatically when the test executable starts; a missing or incompatible fetcher DLL causes startup to
fail before any tests run.

## Simulated-game tests

`tests/data/` contains recorded games from DOS SWOS, captured with the special swos-port helper edition of
SWOS++ in `tools/swospp`, in `.rgd` files. This helper is not the normal DOS SWOS++ release and its
port-specific tree belongs in `swos-port`; it must not be pushed to the separate
`github.com/zlatkok/swospp` repository. Each
recording contains the input state and important structures and variables for every frame. The
`RecordedDataTest` harness initializes a mocked, non-graphical game, replays the inputs at full speed and
compares its state with the recorded state after each frame.

The recordings were selected while observing code coverage, with the aim of exercising as many game-logic
branches as practical. A mismatch generally means that a ported routine no longer behaves exactly like the
original DOS implementation; the failing recording and frame provide the starting point for debugging.

These tests are the project's strongest safeguard against behavioral drift from the original game. Rather
than relying only on whether a match looks or feels right, they use the DOS executable's recorded internal
state as a frame-by-frame behavioral oracle. This provides unusually strong evidence of compatibility and
is an important distinction between `swos-port` and recreations validated primarily through manual
play-testing. Run the recorded-game suite after changing match logic, even when the visible behavior
appears unchanged: small differences can accumulate for many frames before becoming noticeable during play.

The `swos-data-layout` suite protects VM layout assumptions that ordinary C++ type checks cannot cover. It
checks fixed offsets within the career and competition file-loading areas, exported buffer sizes, preserved
original frame-table packing, and known offsets into the chairman-scenes string pool. Keep it in sync with
the documented file formats and layout-sensitive annotations in `swos/symbols.txt`.

## Python tests

The `mnu2h` suite uses `unittest` plus the third-party `ddt` package. Run it from the `mnu2h` directory so
its modules are importable:

```powershell
cd mnu2h
python -m unittest discover -s tests -p "Test*.py"
```

Install `ddt` in the active Python environment first if it is not already available.

The animation-table generator tests use only the Python standard library and may be run from the repository
root:

```powershell
python src/game/animation/testGenerateAnimationTables.py
```

These Python suites are independent of the C++ executable and are not registered as Meson tests. Run the
relevant suite whenever changing the menu compiler or animation-table parser/generator.

## Token lookup tester

The `token-lookup-tester` project validates the tokenizer code generated by
`ida2asm/gen-lookup/gen-lookup.py` from `ida2asm/gen-lookup/tokens.lst`. It checks the generated
switch-based lookup against the expected token text, type, category and subcategory. The generated
executable also benchmarks the switch and `if` state machines, `std::unordered_map`, a custom hash table
and, when available at generation time, a `gperf` lookup.

These comparisons informed the tokenizer design: `ida2asm` uses the generated switch-based state machine,
which was selected as the fastest implementation during development.

Build the `token-lookup-tester` project in `project/vc++/swos-port.sln`, then run the executable without
arguments. For example:

```powershell
.\tmp\token-lookup-tester-x64-Debug\token-lookup-tester.exe
```

The tester exits with a non-zero status on a correctness failure. On success it prints hash-collision
counts and timings for each implementation. Prefer a Release build when comparing timings; Debug results
are still useful for correctness but not meaningful as performance measurements. The project is built as
part of the solution but is not run automatically or registered with Meson.

The correctness test does not print a separate success message. If execution reaches the benchmark report
and the process returns zero, every generated token was recognized with the expected text, length, type,
category and subcategory by all tested implementations. A failure instead identifies the token index and
prints the expected and actual fields before returning a non-zero status.
