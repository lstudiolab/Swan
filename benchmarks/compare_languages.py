#!/usr/bin/env python3
"""Comparable compile/runtime benchmark across nine languages.

Every implementation performs the same 50M-iteration int32 recurrence and
prints the same checksum. Compile time is a clean one-shot build measurement;
runtime is the median of five runs after one warm-up. Python is interpreted,
so its ahead-of-time compile time is N/A.
"""
from pathlib import Path
from statistics import median
import os
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SWAN = Path(os.environ.get("SWAN_BIN", ROOT / "build" / "swan"))
CC = os.environ.get("CC", "clang")
CXX = os.environ.get("CXX", "clang++")
GOOSE = os.environ.get("GOOSE_BIN", "goose")
RUSTC = os.environ.get("RUSTC", "rustc")
SWIFTC = os.environ.get("SWIFTC", "swiftc")
ZIG = os.environ.get("ZIG", "zig")
GO = os.environ.get("GO", "go")
PYTHON = os.environ.get("PYTHON", "python3")
ITERATIONS = 50_000_000
MODULUS = 1_000_000_007
RUNTIME_REPEATS = 5
TIME = shutil.which("time") or "/usr/bin/time"

SWAN_SOURCE = f"""fn main() -> int {{
    var i: int = 0;
    var acc: int = 0;
    while i < {ITERATIONS} {{
        acc = (acc + i) % {MODULUS};
        i = i + 1;
    }}
    log acc;
    return 0;
}}
"""
GOOSE_SOURCE = f"""fn main() {{
    var i: i32 = 0;
    var acc: i32 = 0;
    while i < {ITERATIONS} {{
        acc = (acc + i) % {MODULUS};
        i = i + 1;
    }}
    print(acc);
}}
"""
RUST_SOURCE = f"""fn main() {{
    let mut i: i32 = 0;
    let mut acc: i32 = 0;
    while i < {ITERATIONS} {{
        acc = (acc + i) % {MODULUS};
        i += 1;
    }}
    println!("{{}}", acc);
}}
"""
C_SOURCE = f"""#include <stdint.h>
#include <stdio.h>
int main(void) {{
    int32_t i = 0, acc = 0;
    while (i < {ITERATIONS}) {{
        acc = (acc + i) % {MODULUS};
        ++i;
    }}
    printf("%d\\n", (int)acc);
    return 0;
}}
"""
CPP_SOURCE = f"""#include <cstdint>
#include <iostream>
int main() {{
    std::int32_t i = 0, acc = 0;
    while (i < {ITERATIONS}) {{
        acc = (acc + i) % {MODULUS};
        ++i;
    }}
    std::cout << acc << '\\n';
}}
"""
SWIFT_SOURCE = f"""var i: Int32 = 0
var acc: Int32 = 0
while i < {ITERATIONS} {{
    acc = (acc + i) % {MODULUS}
    i += 1
}}
print(acc)
"""
ZIG_SOURCE = f"""const std = @import("std");
pub fn main() !void {{
    var i: i32 = 0;
    var acc: i32 = 0;
    while (i < {ITERATIONS}) : (i += 1) {{
        acc = @mod(acc + i, {MODULUS});
    }}
    const out = std.io.getStdOut().writer();
    try out.print("{{d}}\\n", .{{acc}});
}}
"""
GO_SOURCE = f"""package main
import "fmt"
func main() {{
    var i int32 = 0
    var acc int32 = 0
    for i < {ITERATIONS} {{
        acc = (acc + i) % {MODULUS}
        i++
    }}
    fmt.Println(acc)
}}
"""
PYTHON_SOURCE = f"""i = 0
acc = 0
while i < {ITERATIONS}:
    acc = (acc + i) % {MODULUS}
    i += 1
print(acc)
"""


def require_tool(name, command):
    if shutil.which(command) is None and not Path(command).exists():
        raise SystemExit(f"Missing {name} tool: {command}")


def measured(command, cwd=None, extra_env=None):
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    with tempfile.NamedTemporaryFile(mode="r+") as usage:
        started = time.perf_counter()
        result = subprocess.run(
            [TIME, "-f", "%M", "-o", usage.name, *map(str, command)],
            cwd=cwd, env=env, text=True, capture_output=True, check=True,
        )
        elapsed = time.perf_counter() - started
        usage.seek(0)
        peak_kib = int(usage.read().strip() or "0")
    return result.stdout.strip(), elapsed, peak_kib / 1024.0


def compile_once(commands, env=None):
    started = time.perf_counter()
    peak = 0.0
    for command in commands:
        _, _, memory = measured(command, extra_env=env)
        peak = max(peak, memory)
    return time.perf_counter() - started, peak


def run_samples(executable, expected, prefix=None, env=None):
    samples = []
    peak = 0.0
    command = ([*prefix, str(executable)] if prefix else [str(executable)])
    for index in range(RUNTIME_REPEATS + 1):
        output, elapsed, memory = measured(command, extra_env=env)
        if output != expected:
            raise SystemExit(
                f"{executable} produced {output!r}; expected checksum {expected!r}"
            )
        peak = max(peak, memory)
        if index:
            samples.append(elapsed)
    return median(samples), peak


def write(work, name, suffix, content):
    path = work / f"{name}{suffix}"
    path.write_text(content, encoding="utf-8")
    return path


def main():
    tools = [
        ("Swan compiler", str(SWAN)), ("C compiler", CC), ("C++ compiler", CXX),
        ("Rust compiler", RUSTC), ("Goose compiler", GOOSE),
        ("Swift compiler", SWIFTC), ("Zig compiler", ZIG), ("Go compiler", GO),
        ("Python", PYTHON),
    ]
    if not SWAN.exists():
        raise SystemExit(f"Swan compiler not found: {SWAN}; run sh build.sh first")
    for label, command in tools[1:]:
        require_tool(label, command)
    if not Path(TIME).exists():
        raise SystemExit("GNU time is required for peak RSS measurements")

    with tempfile.TemporaryDirectory(prefix="swan-nine-language-") as temp:
        work = Path(temp)
        swan_src = write(work, "loop", ".swan", SWAN_SOURCE)
        goose_src = write(work, "loop", ".goose", GOOSE_SOURCE)
        rust_src = write(work, "loop", ".rs", RUST_SOURCE)
        c_src = write(work, "loop", ".c", C_SOURCE)
        cpp_src = write(work, "loop", ".cpp", CPP_SOURCE)
        swift_src = write(work, "loop", ".swift", SWIFT_SOURCE)
        zig_src = write(work, "loop", ".zig", ZIG_SOURCE)
        go_src = write(work, "loop", ".go", GO_SOURCE)
        python_src = write(work, "loop", ".py", PYTHON_SOURCE)

        outputs = {name: work / name for name in (
            "swan", "rust", "goose", "c", "cpp", "swift", "zig", "go"
        )}
        generated_c = work / "swan-generated.c"
        goose_c = work / "goose-generated.c"

        compile_specs = [
            ("Swan", [
                [SWAN, swan_src, "-o", generated_c],
                [CC, "-std=c11", "-O2", "-DNDEBUG", "-march=native",
                 generated_c, "-o", outputs["swan"]],
            ], outputs["swan"]),
            ("Rust", [[RUSTC, "-C", "opt-level=3", "-C", "target-cpu=native",
                       rust_src, "-o", outputs["rust"]]], outputs["rust"]),
            ("Goose", [[GOOSE, "build", "--cc", CC, "-o", outputs["goose"],
                        goose_src]], outputs["goose"]),
            ("C", [[CC, "-std=c11", "-O2", "-DNDEBUG", "-march=native",
                    c_src, "-o", outputs["c"]]], outputs["c"]),
            ("C++", [[CXX, "-std=c++20", "-O2", "-DNDEBUG", "-march=native",
                      cpp_src, "-o", outputs["cpp"]]], outputs["cpp"]),
            ("Swift", [[SWIFTC, "-O", swift_src, "-o", outputs["swift"]]],
             outputs["swift"]),
            ("Zig", [[ZIG, "build-exe", "-O", "ReleaseFast", "-femit-bin=" +
                      str(outputs["zig"]), zig_src]], outputs["zig"]),
            ("Go", [[GO, "build", "-trimpath", "-o", outputs["go"], go_src]],
             outputs["go"]),
        ]

        compile_results = {}
        for name, commands, _ in compile_specs:
            duration, peak = compile_once(commands)
            compile_results[name] = (duration, peak)

        # Python has no native executable compile step in this comparison.
        expected = None
        checksums = {}
        for name, executable in outputs.items():
            output, _, _ = measured([executable])
            checksums[name] = output
            if expected is None:
                expected = output
        py_output, _, _ = measured([PYTHON, python_src])
        checksums["Python"] = py_output
        if len(set(checksums.values())) != 1:
            raise SystemExit(f"Checksum mismatch across languages: {checksums}")

        print(f"Workload: {ITERATIONS:,} iterations; int32 recurrence")
        print(f"Checksum: {expected}")
        print(f"Runtime: median of {RUNTIME_REPEATS} runs after one warm-up")
        print("Compile timings are one clean build per language/toolchain invocation.")
        print()
        print("| Language | Compile pipeline (s) | Runtime (s) | Runtime peak RSS (MiB) |")
        print("|---|---:|---:|---:|")

        rows = []
        for name, _, executable in compile_specs:
            runtime, memory = run_samples(executable, expected)
            rows.append((name, compile_results[name][0], runtime, memory))
        py_runtime, py_memory = run_samples(
            python_src, expected, prefix=[PYTHON]
        )
        rows.append(("Python (interpreted)", None, py_runtime, py_memory))

        for name, compile_seconds, runtime, memory in rows:
            compile_text = "N/A (interpreted)" if compile_seconds is None else f"{compile_seconds:.4f}"
            print(f"| {name} | {compile_text} | {runtime:.4f} | {memory:.2f} |")

        print()
        print("Correctness: all nine implementations produced the same checksum.")
        print("Compile memory is intentionally omitted: some drivers spawn child compilers,")
        print("so parent-process RSS would not be comparable across toolchains.")


if __name__ == "__main__":
    main()
