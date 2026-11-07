#!/usr/bin/env python3
"""Repeatable end-to-end Swan frontend/codegen benchmarks."""
from pathlib import Path
from statistics import median
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "swan"
REPEATS = 7


def fixtures():
    declarations = 2500
    lines = ["fn main() {"]
    lines.extend(f"    let value{i}: int = {i}" for i in range(declarations))
    lines.extend([f"    log value{declarations - 1}", "}"])
    yield "declarations", "\n".join(lines) + "\n"

    assignments = 1500
    lines = ["fn main() {", "    var total: int = 0"]
    lines.extend("    total = total + 1" for _ in range(assignments))
    lines.extend(["    log total", "}"])
    yield "expressions", "\n".join(lines) + "\n"

    calls = 1000
    lines = [
        "fn increment(value: int) -> int {",
        "    return value + 1",
        "}",
        "fn main() {",
        "    var total: int = 0",
    ]
    lines.extend("    total = increment(total)" for _ in range(calls))
    lines.extend(["    log total", "}"])
    yield "function-calls", "\n".join(lines) + "\n"

    strings = 1200
    lines = ["fn main() {"]
    lines.extend(f'    log "literal-{i}-swan-compiler-benchmark"' for i in range(strings))
    lines.append("}")
    yield "string-literals", "\n".join(lines) + "\n"

    depth = 48
    lines = ["fn main() {"]
    lines.extend("    " * (i + 1) + "if true {" for i in range(depth))
    lines.append("    " * (depth + 1) + "log 1")
    lines.extend("    " * (i + 1) + "}" for i in reversed(range(depth)))
    lines.append("}")
    yield "nested-scopes", "\n".join(lines) + "\n"


def main():
    if not BINARY.exists():
        subprocess.run(["sh", str(ROOT / "build.sh")], check=True)

    with tempfile.TemporaryDirectory(prefix="swan-bench-") as directory:
        work = Path(directory)
        print(f"compiler: {BINARY}")
        print(f"repetitions: {REPEATS}; statistic: median wall time")
        print("| Workload | Median (ms) | Range (ms) |")
        print("|---|---:|---:|")
        for name, source_text in fixtures():
            source = work / f"{name}.swan"
            output = work / f"{name}.c"
            source.write_text(source_text, encoding="utf-8")
            samples = []
            for _ in range(REPEATS):
                started = time.perf_counter()
                subprocess.run(
                    [str(BINARY), str(source), "-o", str(output)],
                    check=True, stdout=subprocess.DEVNULL,
                )
                samples.append((time.perf_counter() - started) * 1000)
            print(
                f"| {name} | {median(samples):.2f} "
                f"| {min(samples):.2f}–{max(samples):.2f} |"
            )


if __name__ == "__main__":
    main()
