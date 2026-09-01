#!/usr/bin/env python3
"""Program a CMake-built firmware image with OpenOCD."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONFIGS = ("interface/stlink.cfg", "target/stm32f4x.cfg")
IMAGE_SUFFIXES = {".elf", ".hex", ".bin"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-p",
        "--preset",
        default=os.environ.get("CMAKE_BUILD_PRESET", "Debug"),
        help="Build preset whose output should be searched (default: %(default)s).",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Directory to search; overrides the preset-based default.",
    )
    parser.add_argument(
        "-i",
        "--image",
        type=Path,
        help="Firmware image to program explicitly.",
    )
    parser.add_argument(
        "-f",
        "--config",
        action="append",
        dest="configs",
        help="OpenOCD config file; can be specified more than once.",
    )
    parser.add_argument(
        "--openocd",
        default=os.environ.get("OPENOCD", "openocd"),
        help="OpenOCD executable (default: %(default)s).",
    )
    return parser.parse_args()


def project_path(path: Path) -> Path:
    return path if path.is_absolute() else (PROJECT_ROOT / path).resolve()


def find_image(build_dir: Path) -> Path:
    if not build_dir.is_dir():
        raise ValueError(f"Build directory not found: {build_dir}")

    images = sorted(
        path
        for path in build_dir.rglob("*")
        if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
    )

    # Prefer ELF because it normally contains symbols and is the natural CMake
    # output. HEX/BIN are fallback formats for projects that do not emit ELF.
    elf_images = [path for path in images if path.suffix.lower() == ".elf"]
    candidates = elf_images or images

    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise ValueError(f"No firmware image found under: {build_dir}")

    names = "\n".join(f"  - {path.relative_to(build_dir)}" for path in candidates)
    raise ValueError(
        f"Multiple firmware images found:\n{names}\nUse --image to choose one."
    )


def main() -> int:
    args = parse_args()
    build_dir = (
        project_path(args.build_dir)
        if args.build_dir
        else PROJECT_ROOT / "build" / args.preset
    )

    try:
        image = project_path(args.image) if args.image else find_image(build_dir)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    if not image.is_file():
        print(f"Firmware image not found: {image}", file=sys.stderr)
        return 1

    command = [args.openocd]
    for config in args.configs or DEFAULT_CONFIGS:
        command.extend(["-f", config])
    command.extend(["-c", f'program "{image.as_posix()}" verify reset exit'])

    print(f"Firmware: {image}")
    print(">", " ".join(command))
    try:
        result = subprocess.run(command, cwd=PROJECT_ROOT).returncode
    except FileNotFoundError:
        print(f"OpenOCD executable not found: {args.openocd}", file=sys.stderr)
        return 127

    if result == 0:
        print("Flash succeeded")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
