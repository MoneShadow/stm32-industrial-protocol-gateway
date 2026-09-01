#!/usr/bin/env python3
"""Remove a CMake build directory."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "build_dir",
        nargs="?",
        default=PROJECT_ROOT / "build",
        type=Path,
        help="Build directory to remove (default: ./build).",
    )
    args = parser.parse_args()

    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = PROJECT_ROOT / build_dir

    # Check the link before resolving it, so a link cannot redirect deletion.
    if build_dir.is_symlink():
        print(f"Refusing to remove symlink: {build_dir}", file=sys.stderr)
        return 1

    build_dir = build_dir.resolve()

    # Avoid turning a typo such as `.` into a project-wide deletion.
    if build_dir == PROJECT_ROOT:
        print("Refusing to remove the project directory.", file=sys.stderr)
        return 1
    if not build_dir.exists():
        print(f"Nothing to clean: {build_dir}")
        return 0
    if not build_dir.is_dir():
        print(f"Not a directory: {build_dir}", file=sys.stderr)
        return 1

    try:
        shutil.rmtree(build_dir)
    except OSError as error:
        print(f"Clean failed: {error}", file=sys.stderr)
        return 1

    print(f"Removed: {build_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
