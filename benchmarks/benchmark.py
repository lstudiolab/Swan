#!/usr/bin/env python3
"""Reproducible Swan CLI benchmark; reports medians, never enforces noisy thresholds."""
from __future__ import annotations

import os
import pathlib
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
CC = os.environ.get("CC", "cc")


def median_ms(command: list[str], repeats: int, cwd: pathlib.Path) -> float:
    samples = []
    for _ in range(repeats):
        start = time.perf_counter()
        result = subprocess.run(
            command, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            text=True, check=False,
        )
        elapsed = (time.perf_counter() - start) * 1000
        if result.returncode:
            raise RuntimeError(
                f"Command failed ({result.returncode}): {' '.join(command)}\n"
                f"{result.stderr}"
            )
        samples.append(elapsed)
    return statistics.median(samples)


def main() -> None:
    if not SWAN.is_file():
        raise SystemExit("Build Swan first with: sh build.sh")

    with tempfile.TemporaryDirectory(prefix="swan-bench-") as directory:
        work = pathlib.Path(directory)
        source = work / "large.swan"
        generated = work / "large.c"
        binary = work / "large"
        function_count = 400

        lines = [
            f"fn value_{index}() -> int:\n    return {index}\n"
            for index in range(function_count)
        ]
        lines.extend([
            "fn main():",
            f"    log value_{function_count - 1}()",
            "    return 0",
            "",
        ])
        source.write_text("\n".join(lines), encoding="utf-8")
        source_bytes = source.stat().st_size

        # Validate once outside timed samples and ensure all benchmark paths agree.
        subprocess.run([str(SWAN), str(source), "--emit-c", "-o", str(generated)],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run([CC, "-std=c11", "-O2", str(generated), "-o", str(binary)],
                       check=True, stdout=subprocess.DEVNULL)
        expected = str(function_count - 1)
        actual = subprocess.check_output([str(binary)], text=True).strip()
        if actual != expected:
            raise RuntimeError(f"Incorrect benchmark result: expected {expected}, got {actual}")

        frontend = median_ms(
            [str(SWAN), str(source), "--emit-c", "-o", str(generated)], 7, work
        )
        native_link = median_ms(
            [CC, "-std=c11", "-O2", str(generated), "-o", str(binary)], 7, work
        )
        end_to_end = median_ms([str(SWAN), str(source)], 5, work)
        runtime = median_ms([str(binary)], 21, work)

        print("Swan CLI benchmark (median wall-clock; lower is better)")
        print(f"Workload: {function_count} functions, {source_bytes:,} source bytes")
        print(f"Frontend + C emission:       {frontend:9.3f} ms")
        print(f"Native C compile + link:     {native_link:9.3f} ms")
        print(f"Direct compile + execute:    {end_to_end:9.3f} ms")
        print(f"Prebuilt executable runtime: {runtime:9.3f} ms")
        print("Correctness: PASS")


if __name__ == "__main__":
    main()
