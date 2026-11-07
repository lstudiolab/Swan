#!/usr/bin/env python3
"""Measure native C backend optimization levels for one Swan program.

This isolates Clang from Swan's frontend so backend compile cost is not
misreported as Swan frontend time. Each level is compiled repeatedly and its
runtime is checked against the same expected output.
"""
from pathlib import Path
from statistics import median
import os
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SWAN = Path(os.environ.get("SWAN_BIN", ROOT / "build" / "swan"))
CC = os.environ.get("CC", "clang")
ITERATIONS = 50_000_000
REPEATS = 7
LEVELS = ("-O0", "-O1", "-O2", "-O3", "-Os")

SOURCE = f"""fn main() -> int {{
    var i: int = 0;
    var acc: int = 0;
    while i < {ITERATIONS} {{
        acc = (acc + i) % 1000000007;
        i = i + 1;
    }}
    log acc;
    return 0;
}}
"""


def elapsed(command):
    started = time.perf_counter()
    result = subprocess.run(command, text=True, capture_output=True, check=True)
    return (time.perf_counter() - started) * 1000.0, result.stdout.strip()


def main():
    if not SWAN.exists():
        raise SystemExit(f"Swan compiler not found: {SWAN}; run sh build.sh first")
    with tempfile.TemporaryDirectory(prefix="swan-backend-") as temp:
        work = Path(temp)
        source = work / "backend.swan"
        generated = work / "backend.c"
        source.write_text(SOURCE, encoding="utf-8")
        elapsed([str(SWAN), str(source), "-o", str(generated)])

        expected = None
        results = []
        for level in LEVELS:
            binary = work / f"swan-{level[1:]}"
            compile_samples = []
            for _ in range(REPEATS):
                duration, _ = elapsed([
                    CC, "-std=c11", level, "-DNDEBUG", "-march=native",
                    str(generated), "-o", str(binary),
                ])
                compile_samples.append(duration)

            runtime_samples = []
            for _ in range(REPEATS):
                duration, output = elapsed([str(binary)])
                if expected is None:
                    expected = output
                elif output != expected:
                    raise SystemExit(
                        f"{level} output {output!r} differs from {expected!r}"
                    )
                runtime_samples.append(duration)
            results.append((level, median(compile_samples), median(runtime_samples)))

        print(f"Workload: {ITERATIONS:,} iterations; checksum: {expected}")
        print(f"Samples: {REPEATS} per compile and runtime; median wall time")
        print("| Clang level | Backend compile (ms) | Program runtime (ms) |")
        print("|---|---:|---:|")
        for level, compile_ms, runtime_ms in results:
            print(f"| {level} | {compile_ms:.2f} | {runtime_ms:.2f} |")


if __name__ == "__main__":
    main()
