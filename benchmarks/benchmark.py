#!/usr/bin/env python3
"""Reproducible Swan compiler and runtime benchmarks; reports medians in seconds."""
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


def median_seconds(command: list[str], repeats: int, cwd: pathlib.Path,
                   expected: str | None = None) -> float:
    samples = []
    for _ in range(repeats):
        start = time.perf_counter()
        result = subprocess.run(
            command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, check=False,
        )
        elapsed = time.perf_counter() - start
        if result.returncode:
            raise RuntimeError(
                f"Command failed ({result.returncode}): {' '.join(command)}\n"
                f"{result.stderr}"
            )
        if expected is not None and result.stdout.strip() != expected:
            raise RuntimeError(
                f"Incorrect result: expected {expected!r}, got {result.stdout.strip()!r}"
            )
        samples.append(elapsed)
    return statistics.median(samples)


def nested_expected_value(limit: int) -> int:
    # Each residue 0..29 occurs 11 times in range(limit); 30..96 occur 10 times.
    # This computes the expected checksum independently in 97^3 operations.
    counts = [limit // 97 + (residue < limit % 97) for residue in range(97)]
    return sum(
        counts[i] * counts[j] * counts[k] * ((31 * i + 17 * j + 13 * k) % 97)
        for i in range(97)
        for j in range(97)
        for k in range(97)
    )


def main() -> None:
    if not SWAN.is_file():
        raise SystemExit("Build Swan first with: sh build.sh")

    with tempfile.TemporaryDirectory(prefix="swan-bench-") as directory:
        work = pathlib.Path(directory)
        source = work / "large.swan"
        generated = work / "large.s"
        binary = work / "large"
        function_count = 400

        lines = [
            f"fn value_{index}() -> int:\n    return {index}\n"
            for index in range(function_count)
        ]
        lines.extend([
            "fn main() -> int:",
            f"    log value_{function_count - 1}()",
            "    return 0",
            "",
        ])
        source.write_text("\n".join(lines), encoding="utf-8")
        source_bytes = source.stat().st_size

        subprocess.run([str(SWAN), str(source), "--emit-asm", "-o", str(generated)],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run([CC, "-x", "assembler", str(generated), "-o", str(binary)],
                       check=True, stdout=subprocess.DEVNULL)
        expected = str(function_count - 1)
        actual = subprocess.check_output([str(binary)], text=True).strip()
        if actual != expected:
            raise RuntimeError(f"Incorrect benchmark result: expected {expected}, got {actual}")

        frontend = median_seconds(
            [str(SWAN), str(source), "--emit-asm", "-o", str(generated)], 7, work
        )
        native_link = median_seconds(
            [CC, "-x", "assembler", str(generated), "-o", str(binary)], 7, work
        )
        end_to_end = median_seconds([str(SWAN), str(source)], 5, work)
        runtime = median_seconds([str(binary)], 21, work, expected)

        # Three nested loops execute 1B bodies. The modulo checksum prevents
        # the C optimizer from replacing the complete loop nest with a constant.
        nested_source = work / "nested_1b.swan"
        nested_binary = work / "nested_1b"
        limit = 1000
        nested_source.write_text(
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
        subprocess.run(
            [str(SWAN), "build", str(nested_source), "-o", str(nested_binary)],
            check=True, stdout=subprocess.DEVNULL,
        )
        nested_expected = str(nested_expected_value(limit))
        nested_runtime = median_seconds(
            [str(nested_binary)], 3, work, nested_expected
        )

        print("Swan benchmark (median wall-clock; seconds; lower is better)")
        print(f"Frontend + assembly emission:       {frontend:.2f} seconds")
        print(f"Assembly + native link:     {native_link:.2f} seconds")
        print(f"Direct compile + execute:    {end_to_end:.2f} seconds")
        print(f"Prebuilt executable runtime: {runtime:.2f} seconds")
        print()
        print("1B nested-loop runtime benchmark")
        print("Workload: 1,000 x 1,000 x 1,000 = 1,000,000,000 iterations")
        print(f"Runtime (median of 3):       {nested_runtime:.2f} seconds")
        print(f"Verified checksum:           {nested_expected}")
        print("Correctness: PASS")


if __name__ == "__main__":
    main()
