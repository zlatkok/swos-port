# Windows demo deployment

> **Development demo:** these archives are unfinished test builds. Packaged graphics, audio, data, and other
> content are placeholders used while developing the port. They are not the final game or a finished release.

`tools/packageWindows.py` creates the Windows x64 zip. It is deliberately separate from normal builds and pull
requests: packaging is run locally on demand or through the manually dispatched `Package Windows x64 demo`
workflow.

## Package types

There are two package types:

- A **full local demo package** contains the executable, generated graphics, runtime DLLs, and original SWOS data
  currently needed by the port. It can run after extraction without pointing at another installation.
- An **engine-only demo package** contains redistributable project outputs and dependencies. Original SWOS data, audio,
  commentary, and music are omitted; start it with `--swos-dir=<existing-installation>`.

The repository carries `assets/demo-data.zip` as the single canonical bundle of non-generated placeholder runtime
content. The manually dispatched GitHub Actions workflow uses it to create the self-contained demo. This content is
provided only for development testing; it does not represent final game data or a finished release.

## Prerequisites

Install Visual Studio 2022 with the C++ build tools and install the Python packages in `assets/requirements.txt`.
The packager invokes MSBuild itself. The `swos-port` project depends on the asset project, so generated atlases and
their compiled-in sprite database are updated before the Release executable is built.

The expected inputs are:

- `bin/x64/swos-port-x64-Release.exe`;
- generated `tmp/assets/{4k,hd,low-res}` directories;
- runtime libraries in `3rd-party/bin/dll/x64` and its `Release` subdirectory;
- the x64 Microsoft Visual C++ runtime, normally discovered from Visual Studio's redist directory.

## Creating a self-contained demo package

From the checked-in placeholder bundle:

```powershell
python tools\packageWindows.py `
  --demo-data-archive assets\demo-data.zip `
  --output dist\swos-port-demo-windows-x64.zip
```

To inspect or refresh that bundle from a known working installation:

```powershell
python tools\createDemoDataArchive.py --swos-dir F:\games\swos
```

The archive is allowlisted and contains only non-generated runtime content. Alternatively, package directly from a
working installation without modifying the canonical archive:

```powershell
python tools\packageWindows.py `
  --swos-dir F:\games\swos `
  --output dist\swos-port-demo-windows-x64-full.zip
```

The script fails instead of silently producing a partial full package. It validates and includes:

- compiled `4k`, `hd`, and `low-res` graphics;
- menu and stadium backgrounds (`swtitle` and `stad`) for all three resolutions;
- SDL, SDL_mixer codec, zlib, and CrashRpt runtime files;
- `msvcp140.dll`, `vcruntime140.dll`, and `vcruntime140_1.dll` for a clean Windows machine;
- `DATA/TEAM.nnn`, `DATA/TEAM.CUS`, `POOLPLYR.DAT`, and the three European competition `.TMD` files;
- `CUSTOMS.EDT`;
- `audio/fx` sound effects;
- either `audio/commentary.zip` or the unpacked `audio/commentary` tree;
- a required `menu` music file and an optional `title` music file, using the same extension preference as the game:
  MP3, Ogg Vorbis, WAV, FLAC, then MIDI. When `title` is absent, menu music starts immediately.

The canonical demo-data archive's `swos.ini` is included automatically as the demo configuration. When packaging
directly from `--swos-dir`, use `--include-user-config` only when intentionally distributing that installation's
`swos.ini`; it is otherwise excluded because controls, paths, and personal options should not leak into a clean
package. Use `--include-symbols` to add the Release PDB.

Each archive contains `README.txt`, a prominent `DEMO-NOTICE.txt`, and a `package-manifest.json` with the size and
SHA-256 hash of every packaged input.

Use `--skip-build` only for diagnostics when intentionally packaging existing outputs. Normal packages always build
first, preventing an older executable from being paired with newer sprite atlases.

## Creating an engine-only package

```powershell
python tools\packageWindows.py --engine-only
```

After extraction, run it against a data installation:

```powershell
swos-port.exe --swos-dir=F:\games\swos
```

In GitHub, open **Actions**, select **Package Windows x64 demo**, choose **Run workflow**, and download the resulting
self-contained demo artifact. This workflow has only `workflow_dispatch`; it does not run for pushes or pull
requests.

## Running after a fresh checkout

The Visual Studio x64 build copies the checked-in SDL, codec, zlib, and CrashRpt runtime files from
`3rd-party/bin/dll/x64` beside the executable in `bin/x64`. This lets the development executable find its DLLs
without a machine-specific `PATH` or `.vcxproj.user` file. A packaged demo additionally carries the app-local
Visual C++ Release runtime needed on a clean machine.

Debug builds still depend on Microsoft's non-redistributable Debug CRT and are intended only for machines with
Visual Studio installed. Only x64 Release is supported by the clean-machine packager.

## Why the allowlist is intentionally small

The development installation at `F:\games\swos` contains editors, DOS executables, test replays, screenshots,
logs, backups, experimental audio, and launcher files. Those are not runtime dependencies and must not be copied
wholesale.

Historical archives under `C:\Users\user\Dropbox\SWOS` confirmed the stable package shape: generated `assets`,
`DATA`, sound effects, commentary, two music tracks, DLLs, and `CUSTOMS.EDT`. Older packages also carried `HARD`,
`SFX`, `GRAFS`, pitch `.DAT`/`.BLK` files, logs, and symbols. The current code renders from generated atlases and
redirects sound loading to `audio/fx`, so those legacy graphics and audio trees are no longer required.

When another original-data format is converted into a repository-owned format, remove it from the full-package
allowlist only after the corresponding runtime loader no longer references it.

## Superseding PR #3

PR #3 proposed checking in the runtime DLL set and documented a manual copy procedure. This deployment replaces
that approach by:

- using its reviewed `3rd-party/bin/dll` location rather than the gitignored local `dll` directory;
- validating inputs instead of relying on a handwritten copy checklist;
- packaging generated assets and the Visual C++ runtime as well as SDL/CrashRpt dependencies;
- explicitly separating redistributable and original-game data;
- providing a manual GitHub Actions artifact without adding a per-PR trigger.
