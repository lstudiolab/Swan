#!/usr/bin/env python3
"""Swan-only stress benchmark: one billion loop-body iterations."""
from __future__ import annotations
import json
import pathlib
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
ITERATIONS = 1_000_000_000
REPEATS = 3

def expected_checksum(n: int) -> int:
    counts = [n // 97 + (r < n % 97) for r in range(97)]
    return sum(
        counts[i] * counts[j] * counts[k] * ((31*i + 17*j + 13*k) % 97)
        for i in range(97) for j in range(97) for k in range(97)
    )

def timed(command: list[str], expected: str | None = None) -> tuple[float, str]:
    start = time.perf_counter()
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(f"{command} failed ({result.returncode}):\n{result.stderr}")
    output = result.stdout.strip()
    if expected is not None and output != expected:
        raise RuntimeError(f"wrong result: expected {expected}, got {output!r}")
    return elapsed, output

def main() -> None:
    subprocess.run(["sh", str(ROOT / "build.sh")], cwd=ROOT, check=True)
    expected = str(expected_checksum(1000))
    with tempfile.TemporaryDirectory(prefix="swan-only-bench-") as td:
        work = pathlib.Path(td)
        source = work / "one-billion.swan"
        assembly = work / "one-billion.s"
        executable = work / "one-billion"
        source.write_text(
            "fn main() -> int:\n"
            "    var total = 0\n"
            "    var i = 0\n"
            "    while i < 1000\n"
            "        var j = 0\n"
            "        while j < 1000\n"
            "            var k = 0\n"
            "            while k < 1000\n"
            "                total = total + ((i * 31 + j * 17 + k * 13) % 97)\n"
            "                k = k + 1\n"
            "            j = j + 1\n"
            "        i = i + 1\n"
            "    log total\n"
            "    return 0\n",
            encoding="utf-8",
        )
        compile_samples = []
        for _ in range(5):
            elapsed, _ = timed([str(SWAN), str(source), "--emit-asm", "-o", str(assembly)])
            compile_samples.append(elapsed)
        subprocess.run(["cc", "-x", "assembler", str(assembly), "-o", str(executable)],
                       cwd=ROOT, check=True)
        # Correctness check before measuring.
        timed([str(executable)], expected)
        runtime_samples = []
        for index in range(REPEATS):
            elapsed, _ = timed([str(executable)], expected)
            runtime_samples.append(elapsed)
            print(f"RUN {index + 1}/{REPEATS}: {elapsed:.6f}s", flush=True)
        report = {
            "compiler": "Swan C11 frontend + Swan-owned x86-64 assembly backend",
            "workload": "three nested loops, 1000 x 1000 x 1000 iterations",
            "iterations": ITERATIONS,
            "expected_checksum": expected,
            "correctness": "PASS",
            "compile_seconds_samples": compile_samples,
            "compile_seconds_median": statistics.median(compile_samples),
            "runtime_seconds_samples": runtime_samples,
            "runtime_seconds_median": statistics.median(runtime_samples),
            "iterations_per_second": ITERATIONS / statistics.median(runtime_samples),
        }
        out = ROOT / "swan-only-benchmark.json"
        out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2), flush=True)

if __name__ == "__main__":
    main()
