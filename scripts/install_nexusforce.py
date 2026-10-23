#!/usr/bin/env python3
"""Build and install NexusForce into a local prefix for NexusWave.

NexusWave links `NexusForce::NexusForce`, so it needs either an installed
NexusForce (this script) or a source checkout configured with
`-DNEXUSWAVE_BUILD_FROM_SOURCE=ON`.

Typical use:

    python3 scripts/install_nexusforce.py
    cmake -B build -DNEXUSWAVE_NEXUSFORCE_PREFIX="$PWD/.deps/nexusforce"
    cmake --build build

The script configures with a temporary `config.json` that turns off the
library's tests/examples/benchmarks/docs to keep the install fast, and always
restores the original file, even on failure or Ctrl-C.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BACKUP_NAME = ".nexuswave-config-backup.json"
DEFAULT_SOURCE = Path(os.environ.get("NEXUSFORCE_SOURCE", REPO_ROOT.parent / "NexusForce-Linux"))
DEFAULT_PREFIX = REPO_ROOT / ".deps" / "nexusforce"
DEFAULT_BUILD = REPO_ROOT / ".build" / "nexusforce"


def log(message: str) -> None:
    print(f"[install-nexusforce] {message}", flush=True)


def fail(message: str, code: int = 1) -> "NoReturn":
    print(f"[install-nexusforce] error: {message}", file=sys.stderr, flush=True)
    raise SystemExit(code)


def normalize_vcpkg_dir(path: Path) -> Path:
    """Return the triplet directory that actually contains share/."""
    if path is None:
        return path
    if (path / "share").is_dir():
        return path
    if path.is_dir():
        for child in sorted(path.iterdir()):
            if child.is_dir() and (child / "share").is_dir():
                return child
    return path


def find_cmake() -> str:
    for candidate in ("cmake", "cmake3"):
        path = shutil.which(candidate)
        if path:
            return path
    fail("cmake not found in PATH")


def restore_stale_backup(source: Path) -> None:
    """Restore config.json if a previous run was interrupted."""
    source_config = source / "config.json"
    backup = source / BACKUP_NAME
    if backup.exists():
        log("restoring config.json from an interrupted previous run")
        shutil.copy2(backup, source_config)
        backup.unlink()


def disable_heavy_targets(source: Path, minimal: bool) -> Path:
    config_path = source / "config.json"
    if not config_path.is_file():
        fail(f"config.json missing under {source}")
    backup = source / BACKUP_NAME
    if backup.exists():
        restore_stale_backup(source)
    shutil.copy2(config_path, backup)
    data = json.loads(config_path.read_text(encoding="utf-8"))
    build = data.setdefault("build", {})
    for key in ("tests", "examples", "benchmarks", "docs"):
        build[key] = False
    if minimal:
        # A terminal application needs none of the database/redis/icu stack, and
        # dropping them keeps the installed CMake package dependency-free apart
        # from zlib.
        dependency = data.setdefault("dependency", {})
        for name in ("postgresql", "mysql", "sqlite3", "hiredis", "icu", "lz4"):
            dependency.setdefault(name, {})["switch"] = False
        feature = data.setdefault("feature", {})
        for name in ("sqlcipher", "io_uring"):
            if name in feature:
                feature[name] = False
    config_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return backup


def run(command: list[str], cwd: Path | None = None) -> None:
    printable = " ".join(command)
    log(printable)
    result = subprocess.run(command, cwd=str(cwd) if cwd else None)
    if result.returncode != 0:
        fail(f"command failed with exit code {result.returncode}: {printable}", result.returncode)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build and install NexusForce for NexusWave")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="NexusForce source tree")
    parser.add_argument("--prefix", type=Path, default=DEFAULT_PREFIX, help="install prefix")
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD, help="build directory")
    parser.add_argument("--config", default="Release", help="CMake build type (default: Release)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4, help="parallel build jobs")
    parser.add_argument("--vcpkg-installed", type=Path, default=None, help="reuse an existing vcpkg_installed dir")
    parser.add_argument("--keep-tests", action="store_true", help="keep the library's tests/examples/benchmarks enabled")
    parser.add_argument("--full-deps", action="store_true", help="keep database/redis/icu dependencies enabled")
    parser.add_argument("--clean", action="store_true", help="delete the build directory first")
    args = parser.parse_args()

    source: Path = args.source.resolve()
    if not (source / "CMakeLists.txt").is_file():
        fail(f"{source} does not look like a NexusForce checkout; pass --source")

    restore_stale_backup(source)
    cmake = find_cmake()

    if args.clean and args.build_dir.exists():
        log(f"removing {args.build_dir}")
        shutil.rmtree(args.build_dir)
    args.build_dir.mkdir(parents=True, exist_ok=True)
    args.prefix.mkdir(parents=True, exist_ok=True)

    backup = None
    if not args.keep_tests:
        backup = disable_heavy_targets(source, minimal=not args.full_deps)

    try:
        configure = [
            cmake,
            "-S",
            str(source),
            "-B",
            str(args.build_dir),
            f"-DCMAKE_BUILD_TYPE={args.config}",
            f"-DCMAKE_INSTALL_PREFIX={args.prefix}",
        ]
        if args.vcpkg_installed is not None:
            configure += [
                "-DVCPKG_MANIFEST_INSTALL=OFF",
                f"-DVCPKG_INSTALLED_DIR={args.vcpkg_installed.resolve()}",
            ]
        run(configure)
        run([cmake, "--build", str(args.build_dir), "--parallel", str(args.jobs)])
        run([cmake, "--install", str(args.build_dir)])
    finally:
        if backup is not None and backup.exists():
            shutil.copy2(backup, source / "config.json")
            backup.unlink()
            log("config.json restored")

    marker = args.prefix / "nexusforce-prefix.txt"
    marker.write_text(f"{args.prefix}\n", encoding="utf-8")

    # The installed package still needs the CMake configs of the libraries it
    # was built against (OpenSSL, pcre2, zlib). Record both prefixes once so
    # consumers only need to include this file.
    prefix_cmake = args.prefix / "prefix.cmake"
    lines = ["# generated by scripts/install_nexusforce.py"]
    lines.append(f'list(APPEND CMAKE_PREFIX_PATH "{args.prefix}")')
    if args.vcpkg_installed is not None:
        lines.append(f'list(APPEND CMAKE_PREFIX_PATH "{normalize_vcpkg_dir(args.vcpkg_installed).resolve()}")')
    prefix_cmake.write_text("\n".join(lines) + "\n", encoding="utf-8")

    log(f"installed NexusForce into {args.prefix}")
    log("configure NexusWave with:")
    log(f'  cmake -B build -DNEXUSWAVE_NEXUSFORCE_PREFIX="{args.prefix}"')
    return 0


if __name__ == "__main__":
    sys.exit(main())
