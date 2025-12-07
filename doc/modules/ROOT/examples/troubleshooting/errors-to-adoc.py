#! /usr/bin/env python3

import json
import re
import platform
import subprocess
from pathlib import Path
import tempfile

script_dir = Path(__file__).resolve().parent

repo_dir = subprocess.run(
    ["git", "rev-parse", "--show-toplevel"],
    capture_output=True,
    text=True,
).stdout.strip()


if platform.system() == "Linux":

    def compile(compiler: str):
        def fn(cpp: Path, exe: Path):
            cp = subprocess.run(
                f"{compiler} -I {repo_dir}/include {cpp} -o {exe} -std=c++17",
                shell=True,
                capture_output=True,
                text=True,
            )
            return [] if cp.returncode == 0 else cp.stderr.splitlines()

        return fn

    compilers = {"clang": compile("clang++"), "gcc": compile("g++")}
else:
    boost_dirs = list(Path(r"C:\Boost\include").glob("boost-*"))
    if len(boost_dirs) == 0:
        raise RuntimeError("No Boost installation found in C:\\Boost\\include")
    boost_dir = sorted(boost_dirs)[-1]
    print("Using Boost from", boost_dir)

    def compile(cpp: Path, exe: Path):
        cp = subprocess.run(
            f"cl /std:c++17 /EHsc /I {boost_dir} /I {repo_dir}\\include {cpp} /Fe{exe} /Fo{exe.with_suffix('.obj')}",
            shell=True,
            capture_output=True,
            text=True,
        )
        return [] if cp.returncode == 0 else cp.stdout.splitlines()[1:]

    compilers = {"msvc": compile}

UP_TO_PREFIX = "// up to: "

examples: dict[str, dict[str, list[str]]] = {}

with tempfile.NamedTemporaryFile(
    mode="w+", suffix=".exe", delete_on_close=False
) as exe:
    exe.close()

    for cpp in script_dir.glob("*.cpp"):
        cpp_text = cpp.read_text(encoding="ascii").splitlines()
        try:
            up_to_line = next(
                i for i, line in enumerate(cpp_text) if line.startswith(UP_TO_PREFIX)
            )
            up_to = re.compile(cpp_text[up_to_line][len(UP_TO_PREFIX) :].strip())
        except StopIteration:
            up_to = None

        error_map: dict[str, list[str]]
        try:
            with open(cpp.with_suffix(".json"), encoding="utf8") as f:
                error_map = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            error_map = {}

        for compiler, command in compilers.items():
            if errors := command(cpp, Path(exe.name)):
                for i, error in enumerate(errors):
                    errors[i] = error.replace(str(cpp), "example.cpp")
                    if up_to and up_to.search(error):
                        break
                del errors[i + 1 :]
                error_map[compiler] = errors
            else:
                error_map = {
                    "runtime": subprocess.run(
                        exe.name, shell=True, capture_output=True, text=True
                    ).stderr.splitlines()
                }

            examples[cpp.stem] = error_map

            with open(cpp.with_suffix(".json"), "w", encoding="utf8") as f:
                json.dump(error_map, f, indent=4)

            if not errors:
                break

EXAMPLE_TEMPLATE = r"""
Example:
[source,c++]
----
include::{{examplesdir}}/troubleshooting/{example}.cpp[tag=content]
----

Error:
include::{{examplesdir}}/troubleshooting/{example}.adoc[]

"""

COMPILATION_ERRORS_TEMPLATE = r"""
{compiler}::
+
```
{errors}
```
"""[1:]

RUNTIME_ERRORS_TEMPLATE = r"""
```
{errors}
```
"""[1:]


with open(script_dir / "fragments.adoc", "w", encoding="utf8") as fragments:
    for example, error_map in examples.items():
        print(EXAMPLE_TEMPLATE.format(example=f"{example}"), file=fragments)
        with open(script_dir / f"{example}.adoc", "w", encoding="utf8") as errors_adoc:
            if errors := error_map.get("runtime"):
                assert len(error_map) == 1, f"invalid json file {example}.json"
                print(
                    RUNTIME_ERRORS_TEMPLATE.format(errors="\n".join(errors)),
                    file=errors_adoc,
                )
            else:
                print("[tabs]", file=errors_adoc)
                print("======", file=errors_adoc)
                for compiler, errors in sorted(error_map.items()):
                    print(
                        COMPILATION_ERRORS_TEMPLATE.format(
                            compiler=compiler, errors="\n".join(errors)
                        ),
                        file=errors_adoc,
                    )
                print("======", file=errors_adoc)

    # for compiler, errors in error_map.items():
