#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys

# ---- low-level helpers -------------------------------------------------------


def run_cmd(cmd, cwd=None):
    print("+", " ".join(cmd))
    try:
        subprocess.check_call(cmd, cwd=cwd)
        return True
    except subprocess.CalledProcessError as err:
        return False


def is_configured(build_dir: str) -> bool:
    return os.path.isfile(os.path.join(build_dir, "CMakeCache.txt"))

# ---- core actions: no argparse here, only plain args -------------------------


def do_gen(build_dir: str,
           build_type: str,
           generator: str,
           cmake_defs: list[str]):
    os.makedirs(build_dir, exist_ok=True)

    cmd = [
        "cmake",
        "-S", ".",
        "-B", build_dir,
        "-G", generator,
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    # extra -D flags passed as-is
    for d in cmake_defs:
        cmd.append(f"-D{d}")

    return run_cmd(cmd)


def do_make(build_dir: str,
            build_type: str,
            generator: str,
            cmake_defs: list[str],
            targets: list[str]):
    # configure if needed
    if not is_configured(build_dir):
        if not do_gen(build_dir, build_type, generator, cmake_defs):
            return False

    cmd = ["cmake", "--build", build_dir]
    if targets:
        cmd.extend(["--target", *targets])
    return run_cmd(cmd)


def do_run(build_dir: str,
           build_type: str,
           generator: str,
           cmake_defs: list[str],
           executable: str,
           exe_args: list[str],
           build_targets: list[str]):
    # make first (optionally only specific targets)
    if not do_make(build_dir, build_type, generator, cmake_defs, build_targets):
        return False

    exe_path = os.path.join(build_dir, "bin", executable)
    if os.name == "nt":
        exe_path += ".exe"

    if not os.path.isfile(exe_path):
        print(f"error: executable '{exe_path}' not found", file=sys.stderr)
        sys.exit(1)

    return run_cmd([exe_path, *exe_args])


def do_test(build_dir: str,
            build_type: str,
            generator: str,
            cmake_defs: list[str],
            label: str | None,
            verbose: bool):
    # ensure tests are built (let CMake decide what that means)
    if not do_make(build_dir, build_type, generator, cmake_defs, targets=[]):
        return False

    cmd = ["ctest", "--output-on-failure"]
    if label:
        cmd.extend(["-L", label])
    if verbose:
        cmd.append("--verbose")
    return run_cmd(cmd, cwd=build_dir)

# ---- CLI layer: just wiring args to core functions ---------------------------


def main():
    parser = argparse.ArgumentParser(prog="pls")

    # common options shared by all subcommands
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument(
        "-B", "--build-dir",
        default="build",
        help="Build directory (default: %(default)s)",
    )
    common.add_argument(
        "-t", "--type",
        dest="build_type",
        default="Debug",
        help="CMAKE_BUILD_TYPE (default: %(default)s)",
    )
    common.add_argument(
        "-G", "--generator",
        default="Ninja",
        help="CMake generator (default: %(default)s)",
    )
    common.add_argument(
        "-D",
        dest="cmake_defs",
        action="append",
        default=[],
        help="Extra -DVAR=VALUE definitions for CMake (can be repeated)",
    )

    subparsers = parser.add_subparsers(dest="cmd", required=True)

    # pls gen
    p_gen = subparsers.add_parser("gen", parents=[common],
                                  help="Configure the CMake project")
    p_gen.set_defaults(
        func=lambda a: do_gen(
            build_dir=a.build_dir,
            build_type=a.build_type,
            generator=a.generator,
            cmake_defs=a.cmake_defs,
        )
    )

    # pls make [targets...]
    p_make = subparsers.add_parser("make", parents=[common],
                                   help="Build the project (optionally specific targets)")
    p_make.add_argument(
        "targets",
        nargs="*",
        help="Targets to build (default: all)",
    )
    p_make.set_defaults(
        func=lambda a: do_make(
            build_dir=a.build_dir,
            build_type=a.build_type,
            generator=a.generator,
            cmake_defs=a.cmake_defs,
            targets=a.targets,
        )
    )

    # pls run [options] -- [args to exe]
    p_run = subparsers.add_parser("run", parents=[common],
                                  help="Build and run an executable from the build dir")
    p_run.add_argument(
        "executable",
        default="example",
        nargs="?",
        help="Name of the executable in the build dir (default: %(default)s)",
    )
    p_run.add_argument(
        "--build-target",
        action="append",
        default=[],
        help="Specific target(s) to build before running (can be repeated). "
             "Default: build all.",
    )
    p_run.add_argument(
        "rest",
        nargs=argparse.REMAINDER,
        help="Arguments to pass to the executable (prefix with --)",
    )

    def run_wrapper(a):
        exe_args = a.rest
        if exe_args and exe_args[0] == "--":
            exe_args = exe_args[1:]
        do_run(
            build_dir=a.build_dir,
            build_type=a.build_type,
            generator=a.generator,
            cmake_defs=a.cmake_defs,
            executable=a.executable,
            exe_args=exe_args,
            build_targets=a.build_target,
        )
    p_run.set_defaults(func=run_wrapper)

    # pls test
    p_test = subparsers.add_parser("test", parents=[common],
                                   help="Build (if needed) and run ctest")
    p_test.add_argument(
        "-L", "--label",
        help="Only run tests with this label",
    )

    p_test.add_argument("-v", "--verbose", action="store_true", help="Show test stdout/stderr")

    p_test.set_defaults(
        func=lambda a: do_test(
            build_dir=a.build_dir,
            build_type=a.build_type,
            generator=a.generator,
            cmake_defs=a.cmake_defs,
            label=a.label,
            verbose=a.verbose,
        )
    )

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
