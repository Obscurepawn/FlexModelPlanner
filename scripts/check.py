#!/usr/bin/env python3
"""Run the dependency-free test suite and check exact negative diagnostics."""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(command, *, input_text=None, expected_status=0, expected_text=None, env=None):
    result = subprocess.run(command, cwd=ROOT, input=input_text, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    output = result.stdout + result.stderr
    if result.returncode != expected_status:
        raise AssertionError(f"exit {result.returncode}, expected {expected_status}\n{output}")
    if expected_text is not None and expected_text not in output:
        raise AssertionError(f"missing expected output {expected_text!r}\n{output}")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-make")
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--cxxflags", default="-std=c++20 -O2")
    parser.add_argument("--ldflags", default="")
    parser.add_argument("--exeext", default=".exe" if os.name == "nt" else "")
    parser.add_argument("--make", default="make")
    args = parser.parse_args()
    build = Path(args.build_dir).resolve()
    compiler = shlex.split(args.cxx)
    flags = shlex.split(args.cxxflags)
    link_flags = shlex.split(args.ldflags)
    executable = lambda name: str(build / (name + args.exeext))
    passed = failed = 0

    def check(name, action):
        nonlocal passed, failed
        try:
            detail = action()
            passed += 1
            print(f"PASS {name}")
            if detail:
                print(detail.strip())
        except (AssertionError, OSError, ValueError) as error:
            failed += 1
            print(f"FAIL {name}: {error}")

    for source in sorted((ROOT / "tests").glob("test_*.cpp")):
        check(source.stem, lambda source=source: run([executable(source.stem)]))

    def negative(source):
        marker = re.search(r"^// EXPECT_ERROR: (.+)$", source.read_text(), re.MULTILINE)
        if marker is None:
            raise AssertionError("missing EXPECT_ERROR marker")
        expected = marker.group(1).strip()
        result = subprocess.run(compiler + flags + ["-I", str(ROOT / "include"),
                                "-fsyntax-only", str(source)], cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = result.stdout + result.stderr
        if result.returncode == 0:
            raise AssertionError("invalid program unexpectedly compiled")
        if expected not in output:
            raise AssertionError(f"compilation failed without {expected!r}\n{output}")

    for source in sorted((ROOT / "tests/compile_fail").glob("*.cpp")):
        check("compile_fail_" + source.stem, lambda source=source: negative(source))

    check("example_recent_trade", lambda: run([executable("recent_trade_vs_mid")],
          expected_text="recent10=1.66667, recent3=1.66667"))
    check("replay_sample", lambda: run([executable("csv_replay"), "examples/sample.csv"],
          expected_text="Q,1.66667,1.66667\nQ,0,0\nQ,2,2\n"))
    check("replay_bad_data", lambda: run([executable("csv_replay"), "examples/invalid.csv"],
          expected_status=1, expected_text="line 2: expected a finite number"))

    def csv_inputs():
        cases = [
            ("equal trades", "B,100,102\nT,103\nT,103\nQ\n", 0, "Q,2,2"),
            ("query before events", "Q\n", 0, "Q,0,0"),
            ("CRLF", "  B,100,102\r\n T,103 \r\n Q\r\n", 0, "Q,2,2"),
            ("NaN", "B,100,102\nT,nan\n", 1, "line 2: expected a finite number"),
            ("infinity", "B,100,102\nT,inf\n", 1, "line 2: expected a finite number"),
            ("trailing text", "B,100,102\nT,103x\n", 1, "line 2: expected a finite number"),
            ("empty field", "B,,102\n", 1, "line 1: expected a finite number"),
            ("extra field", "B,100,102,3\n", 1, "exact field count"),
            ("crossed book", "B,102,100\n", 1, "line 1: book bid exceeds ask"),
            ("trade before book", "T,100\n", 1, "line 1: trade requires a preceding book record"),
            ("unknown record", "X,100\n", 1, "line 1: expected B,bid,ask"),
        ]
        for name, text, status, output in cases:
            try:
                run([executable("csv_replay"), "-"], input_text=text,
                    expected_status=status, expected_text=output)
            except AssertionError as error:
                raise AssertionError(f"{name}: {error}") from error
        return f"{len(cases)} input cases passed"

    check("replay_input_validation", csv_inputs)

    def installed_consumer():
        prefix = build / "install-smoke"
        if prefix.exists():
            shutil.rmtree(prefix)
        # This nested make does no compilation and needs no inherited jobserver.
        environment = os.environ.copy()
        environment.pop("MAKEFLAGS", None)
        environment.pop("MFLAGS", None)
        run(shlex.split(args.make) + ["--no-print-directory", "install",
            "PREFIX=" + prefix.as_posix(), "DESTDIR="], env=environment)
        consumer = executable("install_consumer")
        run(compiler + flags + ["-I", str(prefix / "include"),
            str(ROOT / "tests/install_consumer.cpp"), "-o", consumer] + link_flags)
        run([consumer])
        return "Installed headers compiled and ran in a separate consumer."

    check("install_consumer", installed_consumer)
    print(f"\n{passed} checks passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
