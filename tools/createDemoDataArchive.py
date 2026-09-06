#!/usr/bin/env python3
"""Create the checked-in placeholder-data archive from a local SWOS installation."""

from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

import packageWindows


ROOT = Path(__file__).resolve().parents[1]


def parseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create the SWOS port placeholder demo-data archive.")
    parser.add_argument("--swos-dir", dest="swosDir", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "assets/demo-data.zip")
    return parser.parse_args()


def main() -> int:
    args = parseArgs()
    files: dict[str, packageWindows.PackageFile] = {}
    try:
        metadata = packageWindows.collectSwosData(args.swosDir, files)
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        entries = sorted(files.values(), key=lambda item: str(item.destination).casefold())

        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for item in entries:
                archive.write(item.source, str(item.destination))
            archive.writestr(
                "DEMO-DATA-NOTICE.txt",
                packageWindows.zipBytes(
                    "These files are placeholders for the unfinished SWOS port development demo.\n"
                    "They are not final game content or a finished release.\n"
                ),
            )
            archive.writestr(
                "demo-data-manifest.json",
                json.dumps(metadata, indent=2, sort_keys=True).encode("utf-8") + b"\n",
            )
        print(f"Created {output} with {len(entries)} placeholder files.")
    except (FileNotFoundError, ValueError, OSError, zipfile.BadZipFile) as error:
        print(f"Demo data archive creation failed: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
