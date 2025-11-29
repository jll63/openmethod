#! /usr/bin/env python3

import json
import re
import platform
import subprocess
from pathlib import Path

here = Path(__file__).resolve().parent

repo_dir = subprocess.run(
    ["git", "rev-parse", "--show-toplevel"],
    capture_output=True,
    text=True,
).stdout.strip()


if platform.system() == "Linux":

    def compile(compiler: str):
        def fn(cpp: Path):
            return subprocess.run(
                f"{compiler} -I {repo_dir}/include {cpp}",
                shell=True,
                capture_output=True,
                text=True,
            ).stderr.splitlines()

        return fn

    compilers = {"clang": compile("clang++"), "gcc": compile("g++")}
else:
    boost_dirs = list(Path(r"C:\Boost\include").glob("boost-*"))
    if len(boost_dirs) == 0:
        raise RuntimeError("No Boost installation found in C:\\Boost\\include")
    boost_dir = sorted(boost_dirs)[-1]
    print("Using Boost from", boost_dir)

    def compile(cpp: Path):
        return subprocess.run(
            f"cl /std:c++17 /EHsc /I {boost_dir} /I {repo_dir}\\include {cpp}",
            shell=True,
            capture_output=True,
            text=True,
        ).stdout.splitlines()[1:]

    compilers = {"msvc": compile}

UP_TO_PREFIX = "// up to: "


for cpp in here.glob("*.cpp"):
    cpp_text = cpp.read_text(encoding="ascii").splitlines()
    up_to_line = next(i for i, line in enumerate(cpp_text) if line.startswith(UP_TO_PREFIX))
    up_to = re.compile(cpp_text[up_to_line][len(UP_TO_PREFIX):].strip())

    error_map: dict[str, list[str]]
    try:
        with open(cpp.with_suffix(".json"), encoding="ascii") as f:
            error_map = json.load(f)
    except FileNotFoundError:
        error_map = {}

    for compiler, command in compilers.items():
        cp = subprocess.run(
            f"{command} {cpp}", shell=True, capture_output=True, text=True
        )

        errors = []

        for line in command(cpp):
            errors.append(line.replace(str(cpp), "example.cpp"))
            if up_to.search(line):
                break

        error_map[compiler] = errors

        with open(cpp.with_suffix(".json"), "w", encoding="ascii") as f:
            json.dump(error_map, f, indent=4)
