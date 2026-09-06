#!/usr/bin/env python3
"""Build and package a self-contained Windows SWOS port demo."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]
ASSET_RESOLUTIONS = ("4k", "hd", "low-res")
MUSIC_EXTENSIONS = ("mp3", "ogg", "wav", "flac", "mid")
CORE_DATA_FILES = ("EUROCUP.TMD", "EUROCWC.TMD", "UEFACUP.TMD", "POOLPLYR.DAT")
VC_RUNTIME_FILES = ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")


@dataclass(frozen=True)
class PackageFile:
    source: Path
    destination: PurePosixPath


def parseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Package the Windows x64 Release build into a runnable zip."
    )
    parser.add_argument("--swos-dir", dest="swosDir", type=Path, help="SWOS data installation used for a full package")
    parser.add_argument(
        "--demo-data-archive",
        dest="demoDataArchive",
        type=Path,
        help="checked-in placeholder data archive used for a self-contained demo",
    )
    parser.add_argument(
        "--engine-only",
        dest="engineOnly",
        action="store_true",
        help="omit original-game data; the resulting package needs --swos-dir at runtime",
    )
    parser.add_argument("--exe", type=Path, default=ROOT / "bin/x64/swos-port-x64-Release.exe")
    parser.add_argument("--assets-dir", dest="assetsDir", type=Path, default=ROOT / "tmp/assets")
    parser.add_argument("--runtime-dir", dest="runtimeDir", type=Path, default=ROOT / "3rd-party/bin/dll/x64")
    parser.add_argument("--vc-runtime-dir", dest="vcRuntimeDir", type=Path, help="directory containing the VC runtime DLLs")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/swos-port-demo-windows-x64.zip")
    parser.add_argument("--include-symbols", dest="includeSymbols", action="store_true", help="include the Release PDB when present")
    parser.add_argument("--include-user-config", dest="includeUserConfig", action="store_true", help="include swos.ini from --swos-dir")
    parser.add_argument(
        "--skip-build",
        dest="skipBuild",
        action="store_true",
        help="package existing outputs without building (intended for diagnostics only)",
    )
    return parser.parse_args()


def findMsBuild() -> Path:
    command = shutil.which("msbuild")
    if command:
        return Path(command)

    programFilesX86 = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = programFilesX86 / "Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.is_file():
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.Component.MSBuild",
                "-property",
                "installationPath",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        installationPath = Path(result.stdout.strip())
        candidate = installationPath / "MSBuild/Current/Bin/MSBuild.exe"
        if candidate.is_file():
            return candidate

    raise FileNotFoundError("Could not find MSBuild. Install Visual Studio 2022 with C++ build tools.")


def buildRelease() -> None:
    msbuild = findMsBuild()
    solution = ROOT / "project/vc++/swos-port.sln"
    print("Building Windows x64 Release and its generated assets...")
    subprocess.run(
        [
            str(msbuild),
            str(solution),
            "/m",
            "/nologo",
            "/verbosity:minimal",
            "/t:swos-port",
            "/p:Configuration=Release",
            "/p:Platform=x64",
        ],
        cwd=ROOT,
        check=True,
    )


def validateBuildFreshness(exePath: Path, assetsDir: Path) -> None:
    exePath = requireFile(exePath.resolve(), "Release executable")
    assetsDir = requireDir(assetsDir.resolve(), "compiled asset directory")
    databaseNames = (
        "spriteDatabase.h",
        "variableSprites.h",
        "pitchDatabase.h",
        "stadiumSprites.h",
    )
    databasePaths = [requireFile(assetsDir / name, f"generated asset database {name}") for name in databaseNames]
    newestDatabase = max(databasePaths, key=lambda path: path.stat().st_mtime_ns)
    if exePath.stat().st_mtime_ns < newestDatabase.stat().st_mtime_ns:
        raise ValueError(
            f"Release executable is older than {newestDatabase.name}; its compiled sprite coordinates may not "
            "match the atlases. Re-run without --skip-build."
        )


def requireFile(path: Path, purpose: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"Missing {purpose}: {path}")
    return path


def requireDir(path: Path, purpose: str) -> Path:
    if not path.is_dir():
        raise FileNotFoundError(f"Missing {purpose}: {path}")
    return path


def addFile(files: dict[str, PackageFile], source: Path, destination: str | PurePosixPath) -> None:
    destination = PurePosixPath(destination)
    key = str(destination).casefold()
    if key in files:
        raise ValueError(f"Duplicate package destination: {destination}")
    files[key] = PackageFile(source.resolve(), destination)


def addTree(files: dict[str, PackageFile], source: Path, destination: str) -> int:
    count = 0
    for path in sorted(source.rglob("*"), key=lambda item: item.as_posix().casefold()):
        if path.is_file():
            addFile(files, path, PurePosixPath(destination) / path.relative_to(source).as_posix())
            count += 1
    return count


def findVcRuntimeDir(explicit: Path | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(explicit)

    redist = os.environ.get("VCToolsRedistDir")
    if redist:
        candidates.append(Path(redist) / "x64/Microsoft.VC143.CRT")

    programFiles = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    redistRoot = programFiles / "Microsoft Visual Studio/2022"
    if redistRoot.is_dir():
        candidates.extend(redistRoot.glob("*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT"))

    valid = [path for path in candidates if all((path / name).is_file() for name in VC_RUNTIME_FILES)]
    if not valid:
        names = ", ".join(VC_RUNTIME_FILES)
        raise FileNotFoundError(
            f"Could not find the Visual C++ runtime ({names}). Pass --vc-runtime-dir."
        )
    return sorted(valid, key=lambda path: str(path).casefold())[-1]


def collectEngineFiles(args: argparse.Namespace, files: dict[str, PackageFile]) -> Path:
    exe = requireFile(args.exe.resolve(), "Release executable")
    addFile(files, exe, "swos-port.exe")

    assetsDir = requireDir(args.assetsDir.resolve(), "compiled asset directory")
    for resolution in ASSET_RESOLUTIONS:
        source = requireDir(assetsDir / resolution, f"compiled {resolution} assets")
        if addTree(files, source, f"assets/{resolution}") == 0:
            raise FileNotFoundError(f"No files found in {source}")

    runtimeDir = requireDir(args.runtimeDir.resolve(), "runtime DLL directory")
    releaseDir = requireDir(runtimeDir / "Release", "Release runtime DLL directory")
    for source in sorted(runtimeDir.iterdir(), key=lambda item: item.name.casefold()):
        if source.is_file():
            addFile(files, source, source.name)
    for source in sorted(releaseDir.iterdir(), key=lambda item: item.name.casefold()):
        if source.is_file():
            addFile(files, source, source.name)

    vcRuntimeDir = findVcRuntimeDir(args.vcRuntimeDir)
    for name in VC_RUNTIME_FILES:
        addFile(files, vcRuntimeDir / name, name)

    if args.includeSymbols:
        pdb = exe.with_suffix(".pdb")
        addFile(files, requireFile(pdb, "Release symbols"), pdb.name)

    return vcRuntimeDir


def findCaseInsensitive(directory: Path, name: str) -> Path | None:
    wanted = name.casefold()
    return next((path for path in directory.iterdir() if path.name.casefold() == wanted), None)


def collectSwosData(swosDir: Path, files: dict[str, PackageFile]) -> dict[str, object]:
    swosDir = requireDir(swosDir.resolve(), "SWOS data directory")
    dataDir = requireDir(swosDir / "DATA", "SWOS DATA directory")

    dataFiles: list[Path] = []
    for name in CORE_DATA_FILES:
        source = findCaseInsensitive(dataDir, name)
        dataFiles.append(requireFile(source or dataDir / name, f"DATA/{name}"))

    teamPattern = re.compile(r"TEAM\.(?:\d{3}|CUS)$", re.IGNORECASE)
    teamFiles = [path for path in dataDir.iterdir() if path.is_file() and teamPattern.fullmatch(path.name)]
    if not teamFiles:
        raise FileNotFoundError(f"No TEAM.nnn files found in {dataDir}")
    dataFiles.extend(teamFiles)
    for source in sorted(set(dataFiles), key=lambda item: item.name.casefold()):
        addFile(files, source, PurePosixPath("DATA") / source.name.upper())

    customs = findCaseInsensitive(swosDir, "CUSTOMS.EDT")
    addFile(files, requireFile(customs or swosDir / "CUSTOMS.EDT", "CUSTOMS.EDT"), "CUSTOMS.EDT")

    backgroundSources: dict[str, str] = {}
    for resolution in ASSET_RESOLUTIONS:
        sourceDir = requireDir(swosDir / "assets" / resolution, f"{resolution} background directory")
        for basename in ("swtitle", "stad"):
            source = next(
                (
                    candidate
                    for extension in ("png", "jpg", "jpeg", "bmp")
                    if (candidate := findCaseInsensitive(sourceDir, f"{basename}.{extension}"))
                ),
                None,
            )
            source = requireFile(source or sourceDir / basename, f"assets/{resolution}/{basename} background")
            destination = f"assets/{resolution}/{source.name.lower()}"
            addFile(files, source, destination)
            backgroundSources[f"{resolution}/{basename}"] = source.name

    audioDir = requireDir(swosDir / "audio", "audio directory")
    fxDir = requireDir(audioDir / "fx", "audio/fx directory")
    if addTree(files, fxDir, "audio/fx") == 0:
        raise FileNotFoundError(f"No sound effects found in {fxDir}")

    commentaryZip = findCaseInsensitive(audioDir, "commentary.zip")
    commentaryDir = audioDir / "commentary"
    if commentaryZip and commentaryZip.is_file():
        addFile(files, commentaryZip, "audio/commentary.zip")
        commentarySource = "audio/commentary.zip"
    elif commentaryDir.is_dir() and addTree(files, commentaryDir, "audio/commentary"):
        commentarySource = "audio/commentary/"
    else:
        raise FileNotFoundError("Missing audio/commentary.zip or audio/commentary directory")

    music: dict[str, str] = {}
    for basename in ("title", "menu"):
        source = next(
            (
                candidate
                for extension in MUSIC_EXTENSIONS
                if (candidate := findCaseInsensitive(swosDir, f"{basename}.{extension}"))
            ),
            None,
        )
        if basename == "title" and not source:
            continue
        source = requireFile(source or swosDir / basename, f"{basename} music")
        addFile(files, source, source.name.lower())
        music[basename] = source.name

    return {
        "source": "user-supplied SWOS installation",
        "teamFiles": len(teamFiles),
        "commentary": commentarySource,
        "backgrounds": backgroundSources,
        "music": music,
    }


def extractDemoDataArchive(archivePath: Path, destination: Path) -> None:
    archivePath = requireFile(archivePath.resolve(), "demo data archive")
    with zipfile.ZipFile(archivePath) as archive:
        for member in archive.infolist():
            path = PurePosixPath(member.filename)
            if path.is_absolute() or ".." in path.parts:
                raise ValueError(f"Unsafe path in demo data archive: {member.filename}")
        archive.extractall(destination)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def zipBytes(text: str) -> bytes:
    return text.replace("\n", "\r\n").encode("utf-8")


def writeZip(
    output: Path,
    files: dict[str, PackageFile],
    engineOnly: bool,
    vcRuntimeDir: Path,
    swosData: dict[str, object] | None,
) -> None:
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    entries = sorted(files.values(), key=lambda item: str(item.destination).casefold())
    manifest = {
        "format": 1,
        "platform": "windows-x64",
        "packageType": "engine-only" if engineOnly else "full",
        "vcRuntimeVersion": vcRuntimeDir.parent.parent.name,
        "swosData": swosData,
        "files": [
            {
                "path": str(item.destination),
                "size": item.source.stat().st_size,
                "sha256": sha256(item.source),
            }
            for item in entries
        ],
    }

    if engineOnly:
        instructions = """SWOS port development demo - Windows x64 engine package

THIS IS A DEVELOPMENT DEMO. Its files and content are placeholders used to test
the port. This archive is not the final game or a finished release.

This archive does not contain original SWOS game data, commentary, sound effects,
or music. Run it against an existing installation:

    swos-port.exe --swos-dir=C:\\path\\to\\SWOS

For a self-contained package, run tools/packageWindows.py locally with --swos-dir.
See docs/deployment.md for the required data layout.
"""
    else:
        instructions = """SWOS port development demo - Windows x64 full package

THIS IS A DEVELOPMENT DEMO. Its files and content are placeholders used to test
the port. This archive is not the final game or a finished release.

Extract the complete swos-port directory and run swos-port.exe. The archive
contains the currently required original-data subset selected by the packager.
"""

    temporaryOutput = output.with_name(f".{output.name}.tmp")
    try:
        with zipfile.ZipFile(temporaryOutput, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            root = PurePosixPath("swos-port")
            for item in entries:
                archive.write(item.source, str(root / item.destination))
            archive.writestr(str(root / "README.txt"), zipBytes(instructions))
            archive.writestr(
                str(root / "DEMO-NOTICE.txt"),
                zipBytes(
                    "SWOS port is an unfinished development demo.\n"
                    "Packaged graphics, audio, data, and other content are placeholders for testing.\n"
                    "They do not represent the final game or a finished public release.\n"
                ),
            )
            archive.writestr(
                str(root / "package-manifest.json"),
                json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8") + b"\n",
            )
        temporaryOutput.replace(output)
    finally:
        temporaryOutput.unlink(missing_ok=True)

    print(f"Created {output}")
    print(f"Packaged {len(entries)} files ({'engine-only' if engineOnly else 'full'}).")


def main() -> int:
    args = parseArgs()
    modes = sum((args.engineOnly, bool(args.swosDir), bool(args.demoDataArchive)))
    if modes != 1:
        print("Specify exactly one of --engine-only, --swos-dir, or --demo-data-archive.", file=sys.stderr)
        return 2

    try:
        if not args.skipBuild:
            buildRelease()

        validateBuildFreshness(args.exe, args.assetsDir)

        files: dict[str, PackageFile] = {}
        vcRuntimeDir = collectEngineFiles(args, files)
        temporaryDirectory = None
        if args.demoDataArchive:
            temporaryDirectory = tempfile.TemporaryDirectory(prefix="swos-port-demo-data-")
            dataRoot = Path(temporaryDirectory.name)
            extractDemoDataArchive(args.demoDataArchive, dataRoot)
        else:
            dataRoot = args.swosDir

        swosData = None if args.engineOnly else collectSwosData(dataRoot, files)
        if args.demoDataArchive:
            config = findCaseInsensitive(dataRoot, "swos.ini")
            if config:
                addFile(files, config, "swos.ini")
        elif args.includeUserConfig and args.swosDir:
            config = findCaseInsensitive(args.swosDir.resolve(), "swos.ini")
            if config:
                addFile(files, config, "swos.ini")
        writeZip(args.output, files, args.engineOnly, vcRuntimeDir, swosData)
        if temporaryDirectory:
            temporaryDirectory.cleanup()
    except (FileNotFoundError, ValueError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print(f"Packaging failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
