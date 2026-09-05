#!/usr/bin/env python3
"""Configure and build a CMake project using a preset."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-p",
        "--preset",
        default=os.environ.get("CMAKE_BUILD_PRESET", "Debug"),
        help="CMake preset to use (default: %(default)s).",
    )
    parser.add_argument("-t", "--target", help="Build only this target.")
    parser.add_argument(
        "--parallel",
        nargs="?",
        type=int,
        const=0,
        help="Build in parallel; optionally specify the job count.",
    )
    parser.add_argument(
        "--no-configure",
        action="store_true",
        help="Skip the configure step and use the existing build tree.",
    )
    return parser.parse_args()


def run(command: list[str]) -> int:
    print(">", " ".join(command))
    try:
        return subprocess.run(command, cwd=PROJECT_ROOT).returncode
    except FileNotFoundError:
        print(f"Command not found: {command[0]}", file=sys.stderr)
        return 127


def main() -> int:
    args = parse_args()
    has_presets = (PROJECT_ROOT / "CMakePresets.json").is_file()

    if has_presets:
        configure_command = ["cmake", "--preset", args.preset]
        build_command = ["cmake", "--build", "--preset", args.preset]
    else:
        build_dir = PROJECT_ROOT / "build" / args.preset
        configure_command = [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build_dir),
            f"-DCMAKE_BUILD_TYPE={args.preset}",
        ]
        build_command = ["cmake", "--build", str(build_dir)]

    if not args.no_configure:
        result = run(configure_command)
        if result:
            return result

    if args.target:
        build_command.extend(["--target", args.target])
    if args.parallel is not None:
        build_command.append("--parallel")
        if args.parallel:
            build_command.append(str(args.parallel))

    result = run(build_command)
    if result == 0:
        print("Build succeeded")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
