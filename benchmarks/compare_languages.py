#!/usr/bin/env python3
"""Compare Swan, Rust, and Goose on one equivalent scalar-loop workload.

Compile measurements include each language frontend and the shared Clang C
backend for Swan/Goose. Runtime measurements execute optimized native binaries.
GNU time reports peak RSS in KiB. Results are diagnostic, not universal claims.
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
GOOSE = os.environ.get("GOOSE_BIN", "goose")
RUSTC = os.environ.get("RUSTC", "rustc")
CC = os.environ.get("CC", "clang")
ITERATIONS = 50_000_000
REPEATS = 7
TIME = shutil.which("time") or "/usr/bin/time"

SWAN_SOURCE = f"""fn main() -> int {{
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
RUST_SOURCE = f"""fn main() {{
    let mut i: i32 = 0;
    let mut acc: i32 = 0;
    while i < {ITERATIONS} {{
        acc = (acc + i) % 1000000007;
        i += 1;
    }}
    println!("{{}}", acc);
}}
"""
GOOSE_SOURCE = f"""fn main() {{
    var i: i32 = 0;
    var acc: i32 = 0;
    while i < {ITERATIONS} {{
        acc = (acc + i) % 1000000007;
        i = i + 1;
    }}
    print(acc);
}}
"""


def measured(command, cwd=None):
    with tempfile.NamedTemporaryFile(mode="r+") as usage:
        started = time.perf_counter()
        result = subprocess.run(
            [TIME, "-f", "%M", "-o", usage.name, *map(str, command)],
            cwd=cwd, text=True, capture_output=True, check=True,
        )
        elapsed = time.perf_counter() - started
        usage.seek(0)
        peak_kib = int(usage.read().strip() or "0")
    return result.stdout, elapsed, peak_kib / 1024.0


def compile_pipeline(name, commands):
    elapsed = 0.0
    peak = 0.0
    for command in commands:
        _, duration, memory = measured(command)
        elapsed += duration
        peak = max(peak, memory)
    return {"name": name, "compile_seconds": elapsed, "compile_peak_mb": peak}


def runtime(executable, expected):
    samples = []
    for _ in range(REPEATS + 1):
        output, elapsed, _ = measured([executable])
        if output.strip() != expected:
            raise SystemExit(
                f"{executable} returned {output.strip()!r}; expected {expected!r}"
            )
        samples.append(elapsed)
    # First run is a warm-up; RSS is measured separately from the final sample.
    _, _, peak = measured([executable])
    return median(samples[1:]), peak


def main():
    if not SWAN.exists():
        raise SystemExit(f"Swan compiler not found: {SWAN}; run sh build.sh first")
    for tool in (GOOSE, RUSTC, CC):
        if shutil.which(tool) is None and not Path(tool).exists():
            raise SystemExit(f"Required tool not found: {tool}")
    if not Path(TIME).exists():
        raise SystemExit("GNU time is required for peak RSS measurements")

    with tempfile.TemporaryDirectory(prefix="swan-compare-") as temp:
        work = Path(temp)
        swan_src, swan_c, swan_bin = work / "loop.swan", work / "swan.c", work / "swan"
        rust_src, rust_bin = work / "loop.rs", work / "rust"
        goose_src, goose_c, goose_bin = work / "loop.goose", work / "goose.c", work / "goose"
        swan_src.write_text(SWAN_SOURCE, encoding="utf-8")
        rust_src.write_text(RUST_SOURCE, encoding="utf-8")
        goose_src.write_text(GOOSE_SOURCE, encoding="utf-8")

        results = []
        results.append(compile_pipeline("Swan", [
            [SWAN, swan_src, "-o", swan_c],
            [CC, "-std=c11", "-O3", "-DNDEBUG", "-march=native", swan_c, "-o", swan_bin],
        ]))
        results.append(compile_pipeline("Rust", [
            [RUSTC, "-C", "opt-level=3", "-C", "target-cpu=native",
             rust_src, "-o", rust_bin],
        ]))
        results.append(compile_pipeline("Goose", [
            [GOOSE, "-o", goose_c, goose_src],
            [CC, "-std=c11", "-O3", "-DNDEBUG", "-march=native", goose_c, "-o", goose_bin],
        ]))

        checksums = {}
        for name, executable in (("Swan", swan_bin), ("Rust", rust_bin), ("Goose", goose_bin)):
            output, _, _ = measured([executable])
            checksums[name] = output.strip()
        if len(set(checksums.values())) != 1:
            raise SystemExit(f"Output mismatch: {checksums}")
        expected = next(iter(checksums.values()))

        print(f"Workload: {ITERATIONS:,} iterations; checksum: {expected}")
        print(f"Runtime samples: {REPEATS} after one warm-up; median wall time")
        print()
        print("| Language | Compile pipeline (s) | Compile peak RSS (MiB) | Runtime (s) | Runtime peak RSS (MiB) |")
        print("|---|---:|---:|---:|---:|")
        for result, executable in zip(results, (swan_bin, rust_bin, goose_bin)):
            run_time, run_memory = runtime(executable, expected)
            print(
                f"| {result['name']} | {result['compile_seconds']:.4f} "
                f"| {result['compile_peak_mb']:.2f} | {run_time:.4f} "
                f"| {run_memory:.2f} |"
            )
        print()
        print("All three executables produced the same checksum.")


if __name__ == "__main__":
    main()
