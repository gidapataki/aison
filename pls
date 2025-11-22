#!/usr/bin/env python3
"""
Developer helper for configuring/building/running with Conan + CMake.

Commands (stored defaults in .buildconfig):
  config [-t debug|release] [-G generator]   Set defaults.
  deps [--force]                             Conan install jsoncpp/1.9.5 for current config.
  gen [--force]                              Configure CMake (uses Conan toolchain if present).
  build                                      Build current config.
  test [--verbose]                           Run ctest.
  run [-- args...]                           Build and run the example target.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import List

CONFIG_PATH = Path(".buildconfig")
DEFAULT_BUILD_TYPE = "Debug"
DEFAULT_GENERATOR = "Ninja"


def run(cmd: List[str], cwd: Path | None = None) -> None:
    info = []
    if cwd is not None:
        info = [f"(cwd = {cwd})"]

    print("+", " ".join(cmd + info))
    subprocess.check_call(cmd, cwd=cwd)


def normalize_build_type(bt: str) -> str:
    val = bt.strip().lower()
    if val in {"debug", "dbg"}:
        return "Debug"
    if val in {"release", "rel"}:
        return "Release"
    raise ValueError("build type must be debug or release")


def load_config() -> tuple[str, str]:
    if CONFIG_PATH.exists():
        try:
            data = json.loads(CONFIG_PATH.read_text())
            bt = data.get("build_type", DEFAULT_BUILD_TYPE)
            gen = data.get("generator", DEFAULT_GENERATOR)
            return bt, gen
        except Exception:
            pass
    return DEFAULT_BUILD_TYPE, DEFAULT_GENERATOR


def save_config(build_type: str, generator: str) -> None:
    CONFIG_PATH.write_text(json.dumps({"build_type": build_type, "generator": generator}))
    print(f"[config] build_type={build_type} generator={generator}")


def paths(build_type: str) -> tuple[Path, Path, Path]:
    bdir = Path("build") / build_type.lower()
    deps = Path("build") / "deps" / build_type.lower()
    toolchain = deps / "conan_toolchain.cmake"
    return bdir, deps, toolchain


def cmd_deps(build_type: str, force: bool) -> None:
    bdir, deps_dir, toolchain = paths(build_type)
    if not force and toolchain.exists():
        print(f"[deps] using existing toolchain at {toolchain}")
        return
    deps_dir.mkdir(parents=True, exist_ok=True)
    print(f"[deps] installing jsoncpp/1.9.5 via Conan → {deps_dir}")
    run([
        "conan",
        "install",
        ".",
        "-s",
        f"build_type={build_type}",
        "-of",
        str(deps_dir),
        "--deployer=full_deploy",
        "--build=missing",
    ])


def cmd_gen(build_type: str, generator: str, force: bool) -> None:
    cmd_deps(build_type, force=False)
    bdir, deps_dir, toolchain = paths(build_type)
    cache = bdir / "CMakeCache.txt"
    if cache.exists() and not force:
        print(f"[gen] already configured in {bdir} (use --force to reconfigure)")
        return
    bdir.mkdir(parents=True, exist_ok=True)
    print(f"[gen] configuring {bdir} (generator={generator}, build_type={build_type})")
    run([
        "cmake",
        "-S",
        ".",
        "-B",
        str(bdir),
        "-G",
        generator,
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DAISON_USE_BUNDLED_JSONCPP=OFF",
        "-DAISON_REQUIRE_EXTERNAL_JSONCPP=ON",
        "-DAISON_BUILD_TESTS=ON",
    ])


def cmd_build(build_type: str) -> None:
    bdir, _, _ = paths(build_type)
    if not (bdir / "CMakeCache.txt").exists():
        cmd_gen(build_type, DEFAULT_GENERATOR, force=False)
    print(f"[build] building in {bdir}")
    run(["cmake", "--build", str(bdir)])


def cmd_test(build_type: str, verbose: bool) -> None:
    bdir, _, _ = paths(build_type)
    if not (bdir / "CMakeCache.txt").exists():
        cmd_gen(build_type, DEFAULT_GENERATOR, force=False)
    print(f"[test] running ctest in {bdir}")
    cmd = ["ctest", "--test-dir", str(bdir), "--output-on-failure"]
    if verbose:
        cmd.append("--verbose")
    run(cmd)


def cmd_run(build_type: str, args: List[str]) -> None:
    bdir, _, _ = paths(build_type)
    if not (bdir / "CMakeCache.txt").exists():
        cmd_gen(build_type, DEFAULT_GENERATOR, force=False)
    print(f"[run] building example in {bdir}")
    run(["cmake", "--build", str(bdir), "--target", "example"])
    exe = bdir / "example"
    if os.name == "nt":
        exe = exe.with_suffix(".exe")
    print(f"[run] executing {exe} {' '.join(args)}")
    run([str(exe), *args])


def main() -> None:
    parser = argparse.ArgumentParser(description="Aison dev helper")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_config = sub.add_parser("config", help="Set defaults")
    p_config.add_argument("-t", "--type", help="debug or release")
    p_config.add_argument("-G", "--generator", help="CMake generator (default Ninja)")

    p_deps = sub.add_parser("deps", help="Install deps via Conan")
    p_deps.add_argument("--force", action="store_true", help="Reinstall even if toolchain exists")

    p_gen = sub.add_parser("gen", help="Configure CMake")
    p_gen.add_argument("--force", action="store_true", help="Reconfigure even if cache exists")

    sub.add_parser("build", help="Build current config")

    p_test = sub.add_parser("test", help="Run ctest in current config")
    p_test.add_argument("--verbose", action="store_true", help="Show test output even on success")

    p_run = sub.add_parser("run", help="Build and run example")
    p_run.add_argument("rest", nargs=argparse.REMAINDER, help="Args after -- passed to example")

    args = parser.parse_args()
    build_type, generator = load_config()

    if args.cmd == "config":
        if args.type:
            build_type = normalize_build_type(args.type)
        if args.generator:
            generator = args.generator
        save_config(build_type, generator)
        return

    print(f"[config] build_type={build_type} generator={generator}")

    if args.cmd == "deps":
        cmd_deps(build_type, force=args.force)
    elif args.cmd == "gen":
        cmd_gen(build_type, generator, force=args.force)
    elif args.cmd == "build":
        cmd_gen(build_type, generator, force=False) if not (paths(build_type)[0] / "CMakeCache.txt").exists() else None
        cmd_build(build_type)
    elif args.cmd == "test":
        cmd_gen(build_type, generator, force=False) if not (paths(build_type)[0] / "CMakeCache.txt").exists() else None
        cmd_test(build_type, verbose=args.verbose)
    elif args.cmd == "run":
        rest = args.rest
        if rest and rest[0] == "--":
            rest = rest[1:]
        cmd_gen(build_type, generator, force=False) if not (paths(build_type)[0] / "CMakeCache.txt").exists() else None
        cmd_run(build_type, rest)
    else:
        parser.error("unknown command")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
    except Exception as exc:  # noqa: BLE001
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
