#!/usr/bin/env python3
"""
Simple build helper for the Aison project.

Commands:
  - config <debug|release> : set the default build type (stored in .plsconfig)
  - deps                   : install deps via Conan for the current build type
  - gen                    : configure CMake (uses Ninja, Conan toolchain if present)
  - make                   : build via CMake in the current build directory

Defaults:
  - build type: Debug
  - build dir:  build/<type>  (e.g., build/debug)
  - deps dir:   build/deps/<type> (Conan output)
"""

import argparse
import json
import os
import subprocess
import sys
from typing import Optional

DEFAULT_BUILD_TYPE = "Debug"
CONFIG_FILE = ".plsconfig"


# --------------------------------------------------------------------------- #
# Utilities
# --------------------------------------------------------------------------- #

def run(cmd: list[str], cwd: Optional[str] = None) -> None:
    info = []
    if cwd is not None:
        info = [f"(cwd = {cwd})"]

    print("+", " ".join(cmd + info))
    subprocess.check_call(cmd, cwd=cwd)


def read_build_type() -> str:
    if os.path.isfile(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            bt = data.get("build_type", DEFAULT_BUILD_TYPE)
            return bt
        except Exception:
            pass
    return DEFAULT_BUILD_TYPE


def write_build_type(bt: str) -> None:
    with open(CONFIG_FILE, "w", encoding="utf-8") as f:
        json.dump({"build_type": bt}, f)


def norm_build_type(val: str) -> str:
    v = val.strip().lower()
    if v in ("debug", "dbg"):
        return "Debug"
    if v in ("release", "rel"):
        return "Release"
    raise ValueError("build type must be debug or release")


def build_dir(bt: str) -> str:
    return os.path.join("build", bt.lower())


def deps_dir(bt: str) -> str:
    return os.path.join("build", "deps", bt.lower())


def conan_toolchain(bt: str) -> str:
    return os.path.join(deps_dir(bt), "conan_toolchain.cmake")


def print_config(bt: str) -> None:
    print(f"Config {bt}")


# --------------------------------------------------------------------------- #
# Commands
# --------------------------------------------------------------------------- #

def cmd_config(args: argparse.Namespace) -> None:
    bt = norm_build_type(args.build_type)
    write_build_type(bt)
    print(f"Build type set to {bt}")


def cmd_deps(args: argparse.Namespace) -> None:
    bt = read_build_type()
    print_config(bt)
    out_dir = deps_dir(bt)
    os.makedirs(out_dir, exist_ok=True)
    run([
        "conan", "install", ".",
        "--output-folder", out_dir,
        "-s", f"build_type={bt}",
        "--build=missing",
    ])
    print(f"Deps installed for {bt} in {out_dir}")


def cmd_gen(args: argparse.Namespace) -> None:
    bt = read_build_type()
    print_config(bt)
    bdir = build_dir(bt)
    os.makedirs(bdir, exist_ok=True)

    cmake_cmd = [
        "cmake",
        "-S", ".",
        "-B", bdir,
        "-G", "Ninja",
        f"-DCMAKE_BUILD_TYPE={bt}",
        "-DAISON_BUILD_TESTS=ON",
    ]

    tc_path = conan_toolchain(bt)
    if os.path.isfile(tc_path):
        cmake_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={tc_path}")
        cmake_cmd.append("-DAISON_USE_BUNDLED_JSONCPP=OFF")

    run(cmake_cmd)
    print(f"Configured {bt} in {bdir}")


def cmd_make(args: argparse.Namespace) -> None:
    bt = read_build_type()
    print_config(bt)
    bdir = build_dir(bt)
    if not os.path.isfile(os.path.join(bdir, "CMakeCache.txt")):
        print("Build directory not configured; running gen first")
        cmd_gen(args)
    run(["cmake", "--build", bdir])


def cmd_test(args: argparse.Namespace) -> None:
    bt = read_build_type()
    print_config(bt)
    bdir = build_dir(bt)
    if not os.path.isfile(os.path.join(bdir, "CMakeCache.txt")):
        print("Build directory not configured; running gen first")
        cmd_gen(args)
    cmd = ["ctest", "--output-on-failure"]
    if args.verbose:
        cmd.append("--verbose")
    run(cmd, cwd=bdir)


def cmd_run(args: argparse.Namespace) -> None:
    bt = read_build_type()
    print_config(bt)
    bdir = build_dir(bt)
    if not os.path.isfile(os.path.join(bdir, "CMakeCache.txt")):
        print("Build directory not configured; running gen first")
        cmd_gen(args)
    # ensure built
    run(["cmake", "--build", bdir, "--target", "example"])
    exe_path = os.path.join(bdir, "bin", "example")
    if os.name == "nt":
        exe_path += ".exe"
    run([exe_path, *args.rest])


# --------------------------------------------------------------------------- #
# CLI wiring
# --------------------------------------------------------------------------- #

def main() -> None:
    parser = argparse.ArgumentParser(prog="pls", description="Aison build helper")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_cfg = sub.add_parser("config", help="Set default build type (debug/release)")
    p_cfg.add_argument("build_type", help="debug or release")
    p_cfg.set_defaults(func=cmd_config)

    p_deps = sub.add_parser("deps", help="Install dependencies via Conan for current config")
    p_deps.set_defaults(func=cmd_deps)

    p_gen = sub.add_parser("gen", help="Configure CMake (uses Ninja)")
    p_gen.set_defaults(func=cmd_gen)

    p_make = sub.add_parser("make", help="Build via CMake in current config")
    p_make.set_defaults(func=cmd_make)

    p_test = sub.add_parser("test", help="Run ctest in current config (builds if needed)")
    p_test.add_argument("-v", "--verbose", action="store_true", help="Show test stdout/stderr")
    p_test.set_defaults(func=cmd_test)

    p_run = sub.add_parser("run", help="Build (if needed) and run the example executable")
    p_run.add_argument("rest", nargs=argparse.REMAINDER, help="Args to pass to example (prefix with --)")
    p_run.set_defaults(func=cmd_run)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
    except Exception as exc:  # noqa: BLE001
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
