#!/usr/bin/env python3
import argparse
import subprocess
import sys
import os
from pathlib import Path

BUILD_DIR = Path("build")
GENERATOR = "Ninja"
CMAKE_BUILD_TYPE = "Debug"
EXECUTABLE = Path(".") / "bin" / "example"


def run(cmd, **kwargs):
    print(">>", " ".join(cmd))
    try:
        subprocess.check_call(cmd, **kwargs)
    except subprocess.CalledProcessError as err:
        sys.exit(1)
        pass


def gen():
    if not BUILD_DIR.exists():
        BUILD_DIR.mkdir(parents=True)
    run([
        "cmake",
        "-B", str(BUILD_DIR),
        "-G", GENERATOR,
        f"-DCMAKE_BUILD_TYPE={CMAKE_BUILD_TYPE}",
        "."
    ])


def make(args):
    if not BUILD_DIR.exists():
        print("Build directory missing → running `pls gen` first.\n")
        gen()
    run(["cmake", "--build", str(BUILD_DIR), "--"] + args)


def run_main(args):
    # ensure build
    make([])

    # Resolve executable path
    exe = BUILD_DIR / EXECUTABLE
    if os.name == "nt":
        exe = exe.with_suffix(".exe")

    if not exe.exists():
        print(f"Error: main executable not found at {exe}")
        sys.exit(1)

    # Run with forwarded arguments
    run([str(exe)] + args)


def main():
    parser = argparse.ArgumentParser(prog="pls")
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("gen")

    make_p = sub.add_parser("make")
    make_p.add_argument("args", nargs=argparse.REMAINDER)

    run_p = sub.add_parser("run")
    run_p.add_argument("args", nargs=argparse.REMAINDER)

    args = parser.parse_args()

    if args.cmd == "gen":
        gen()
    elif args.cmd == "make":
        make(args.args)
    elif args.cmd == "run":
        run_main(args.args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
