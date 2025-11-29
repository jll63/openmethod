#! /usr/bin/env python3

import json
import os
import platform
import subprocess
from pathlib import Path

os.chdir(Path(__file__).resolve().parent)

repo_dir = subprocess.run(
    ["git", "rev-parse", "--show-toplevel"],
    capture_output=True,
    text=True,
).stdout.strip()


clangcc = f"-fsyntax-only -I {repo_dir}/include"

if platform.system() == "Linux":
    compilers = {"clang": f"clang++ {clangcc}", "gcc": f"g++ {clangcc}"}
else:
    boost_dirs = r"C:\Boost\include".glob("boost-*")
    if len(boost_dirs) == 0:
        raise RuntimeError("No Boost installation found in C:\\Boost\\include")
    boost_dir = sorted(boost_dirs)[-1]
    print("Using Boost from", boost_dir)
    compilers = {
        "msvc": r"cl /std:c++17 /EHsc /I {} /I {}\include".format(boost_dir, repo_dir)
    }


for cpp in Path(".").glob("*.cpp"):
    errors: dict[str, list[str]]
    try:
        with open(cpp.with_suffix(".json"), encoding="ascii") as f:
            errors = json.load(f)
    except FileNotFoundError:
        errors = {}

    for compiler, command in compilers.items():
        cp = subprocess.run(
            f"{command} {cpp}", shell=True, capture_output=True, text=True
        )

        errors[compiler] = cp.stderr.splitlines()

        with open(cpp.with_suffix(".json"), "w", encoding="ascii") as f:
            json.dump(errors, f, indent=4)
