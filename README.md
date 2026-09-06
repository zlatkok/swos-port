# swos-port

`swos-port` is an unofficial C++/SDL port of the PC DOS version of **Sensible World of Soccer 96/97**.
The project combines gradually rewritten, idiomatic C++ code with a generated virtual-machine translation of
the original game routines. This allows behavior to be preserved while systems are converted and tested one
piece at a time.

Windows is the primary development platform. An Android project also exists, but the Windows Visual Studio and
Meson builds are the actively maintained desktop build paths.

## Project status

This is a development project rather than a finished drop-in replacement. Much of the game is playable, but
conversion and cleanup are ongoing. Some routines are still mechanically generated from `swos/swos.asm`; these
are marked in their C++ header comments and are intended to be replaced with idiomatic implementations.

Behavioral compatibility is checked with frame-by-frame recordings captured from DOS SWOS. The test harness
replays the same inputs and compares important game state against the recording, making subtle simulation drift
detectable long before it becomes visually obvious.

## Requirements

For the current x64 Windows build:

- Visual Studio 2022 with the **Desktop development with C++** workload;
- Python 3.8 or newer;
- Python packages required by the asset compiler:

  ```powershell
  python -m pip install -r assets/requirements.txt
  ```

- Meson 1.3+ and Ninja, only when using the Meson build:

  ```powershell
  python -m pip install meson ninja
  ```

The patched `PyTexturePacker` used by the asset pipeline is vendored in `3rd-party/PyTexturePacker`. Other
headers, import libraries, and runtime DLLs used by the Windows projects are also kept under `3rd-party/` and
`dll/`.

To copy generated atlases into the game data directory automatically, define `SWOS_DIR` before building. The
asset compiler deploys them to `%SWOS_DIR%\assets` and overwrites older atlas files.

See [the complete build documentation](docs/build.md) for details and optional development tools.

## Building with Visual Studio

Open:

```text
project/vc++/swos-port.sln
```

Select `x64` and either `Debug` or `Release`, then build the solution. The solution automatically:

1. generates the `ida2asm` token lookup;
2. builds an optimized `ida2asm` converter;
3. translates `swos/swos.asm` into VM-backed C++ files;
4. compiles menu definitions;
5. generates animation tables;
6. rebuilds sprite and pitch assets when their inputs change;
7. builds the game and test executables.

The game executable is written below `bin/x64/`. Intermediate and generated files are kept below `tmp/`.

## Packaging the Windows demo

The resulting archive is an unfinished development demo. Its graphics, audio, data, and other content are
placeholders for testing the port; it is not the final game or a finished release.

After building x64 Release, create a self-contained package from the checked-in placeholder data archive with:

```powershell
python tools\packageWindows.py --demo-data-archive assets\demo-data.zip
```

The manually dispatched GitHub Actions workflow creates the same self-contained demo zip; it does not run for pushes
or pull requests. See [Windows deployment](docs/deployment.md) for contents, provenance, and package options.

## Building with Meson

Run these commands from an **x64 Native Tools Command Prompt for Visual Studio**:

```bat
meson setup tmp\meson-debug --backend=ninja --buildtype=debug
meson compile -C tmp\meson-debug

meson setup tmp\meson-release --backend=ninja --buildtype=release
meson compile -C tmp\meson-release
```

Meson performs the same code, menu, animation-table, and asset generation stages as the Visual Studio solution.
Its generated files and executables remain inside the selected build directory.

### One switchable Visual Studio solution from Meson

Meson's VS-lite backend creates separate Ninja trees behind one Visual Studio solution:

```bat
meson setup tmp\meson-vs --genvslite vs2022
```

Open `tmp\meson-vs_vs\swos-port.sln`. Its configuration selector contains `debug`, `debugoptimized`, and
`release`; switching between them does not require closing Visual Studio.

Use the installed `meson.exe` command for this operation. Invoking Meson through
`python -m mesonbuild.mesonmain` is not compatible with the VS-lite generator in current Meson releases.

## Running the game

The port still uses data from an installed copy of SWOS. The data directory can be selected with:

```text
swos-port.exe --swos-dir=<path-to-swos>
```

Run the executable with `--help` to see the command-line options supported by the current build. Runtime DLLs
must either be beside the executable or available through `PATH`; the checked-in Visual Studio debugger setup
already supplies the normal development environment.

## Tests

The Visual Studio solution builds the `tests` project alongside the game. A typical x64 Release invocation is:

```powershell
$env:PATH = "$(Resolve-Path dll\x64\Release);$(Resolve-Path dll\x64);$env:PATH"
tmp\tests-x64-Release\testsRelease.exe --swos-dir=<path-to-swos>
```

The test executable supports selecting individual suites and cases. Important suites include:

- frame-by-frame recorded-game compatibility tests;
- SWOS data-layout and generated-table checks;
- menu and menu-snapshot tests;
- commentary, controls, file-selection, and joypad tests.

There are also Python tests for the menu and animation-table generators and a generated `ida2asm` token-lookup
tester. See [docs/tests.md](docs/tests.md) for commands, debugger behavior, snapshot review, and recorded-data
details.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | Game, platform, rendering, audio, input, and converted C++ code |
| `swos/` | SWOS disassembly input and `ida2asm` symbol/conversion rules |
| `ida2asm/` | IDA assembly-to-C++/assembly converter |
| `mnu2h/` | Menu-definition compiler |
| `assets/` | Source sprites, pitches, metadata, and asset compiler |
| `tests/` | C++ test harness, mocks, recorded games, and test projects |
| `3rd-party/` | Vendored headers, libraries, and patched PyTexturePacker |
| `project/vc++/` | Main Visual Studio solution and projects |
| `docs/` | Build, architecture, reverse-engineering, and subsystem notes |
| `tools/` | Supporting conversion, editor, and maintenance tools |
| `tmp/` | Generated and intermediate files; not source-controlled |

## Documentation

- [Build system and generated dependency chain](docs/build.md)
- [Tests and recorded-data verification](docs/tests.md)
- [`ida2asm` reference](docs/ida2asm.md)
- [`mnu2h` menu compiler](docs/mnu2h.md)
- [Animation-table format and generation](docs/animation-tables.md)
- [SWOS subsystem notes](docs/SWOS/)
- [Rendering notes](docs/rendering.txt)
- [Sound modding](docs/sound-modding.txt)

## Legal notice

This is an unofficial fan project and is not affiliated with Sensible Software or the current owners of the
Sensible World of Soccer name and game assets. Use data files from a legally obtained copy of the game.
