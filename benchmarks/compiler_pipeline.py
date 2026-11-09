#!/usr/bin/env python3
"""Repeatable, stage-by-stage Swan compiler pipeline benchmark.

Build Swan first with `sh build.sh`, then run:
    python3 benchmarks/compiler_pipeline.py
All durations are wall-clock seconds. The benchmark reports medians, not a
single best-case sample. It measures the existing --profile instrumentation;
IR generation includes the SIL bridge and typed-IR fallback together.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_COMPILER = ROOT / "build" / "swan"
PROFILE_HEADER = "Swan compiler pipeline profile (seconds)"
STAGE_LINE = re.compile(
    r"^\s{2}(.+?)\s+([0-9]+(?:\.[0-9]+)?) s(?:\s+\(\s*[0-9]+(?:\.[0-9]+)?%\))?\s*$"
)


def make_workloads(directory: pathlib.Path) -> dict[str, pathlib.Path]:
    workloads: dict[str, pathlib.Path] = {
        "control-flow": ROOT / "benchmarks" / "compiler_pipeline.swan"
    }

    # Every helper has a single scalar return expression, exercising the
    # current SIL-compatible lowering subset. main calls each helper so the
    # reachability pass cannot discard the generated functions.
    sil_lines = [
        f"fn sil_helper_{i}(value: int) -> int:\n    return value + {i + 1}\n"
        for i in range(48)
    ]
    sil_lines.extend(["fn main() -> int:", "    var total = 0"])
    sil_lines.extend(
        f"    total = total + sil_helper_{i}(total)" for i in range(48)
    )
    sil_lines.extend(["    return total", ""])
    sil_source = directory / "sil_scalar.swan"
    sil_source.write_text("\n".join(sil_lines), encoding="utf-8")
    workloads["sil-scalar"] = sil_source

    # A larger call graph and frontend workload. The leaf helpers are simple
    # scalar expressions; the chain forces all declarations to remain live.
    large_lines = [
        "fn chain_0(value: int) -> int:\n    return value + 1\n"
    ]
    for i in range(1, 240):
        large_lines.append(
            f"fn chain_{i}(value: int) -> int:\n"
            f"    return chain_{i - 1}(value) + {i + 1}\n"
        )
    large_lines.extend([
        "fn main() -> int:",
        "    var value = 0",
        "    value = chain_239(value)",
        "    return value",
        "",
    ])
    large_source = directory / "large-call-graph.swan"
    large_source.write_text("\n".join(large_lines), encoding="utf-8")
    workloads["large-call-graph"] = large_source
    return workloads


def parse_profile(stderr: str) -> dict[str, float]:
    if PROFILE_HEADER not in stderr:
        raise RuntimeError(
            "Compiler did not emit its pipeline profile. "
            "Check that --profile is supported by this build."
        )
    values: dict[str, float] = {}
    for line in stderr.splitlines():
        match = STAGE_LINE.match(line)
        if match:
            values[match.group(1).strip()] = float(match.group(2))
    if "Total wall time" not in values:
        raise RuntimeError("Profile is missing Total wall time")
    return values


def run_sample(
    compiler: pathlib.Path,
    source: pathlib.Path,
    output: pathlib.Path,
) -> dict[str, float]:
    command = [
        str(compiler), "build", "--profile", str(source), "-o", str(output)
    ]
    started = time.perf_counter()
    result = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=False,
    )
    external_wall = time.perf_counter() - started
    if result.returncode:
        raise RuntimeError(
            f"Compilation failed for {source.name} "
            f"(exit {result.returncode}):\n{result.stderr}"
        )
    if not output.is_file():
        raise RuntimeError(f"Compiler did not create expected output: {output}")
    profile = parse_profile(result.stderr)
    # Retain the external measurement as a cross-check for process overhead.
    profile["Benchmark process wall time"] = external_wall
    return profile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=pathlib.Path, default=DEFAULT_COMPILER)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--warmups", type=int, default=1)
    args = parser.parse_args()
    if args.repeats < 1 or args.warmups < 0:
        parser.error("--repeats must be >= 1 and --warmups must be >= 0")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}\nBuild it with: sh build.sh")

    with tempfile.TemporaryDirectory(prefix="swan-pipeline-bench-") as temp:
        directory = pathlib.Path(temp)
        workloads = make_workloads(directory)
        results: dict[str, list[dict[str, float]]] = {}
        for workload, source in workloads.items():
            output = directory / f"{workload}.out"
            for _ in range(args.warmups):
                run_sample(compiler, source, output)
            samples = [
                run_sample(compiler, source, output)
                for _ in range(args.repeats)
            ]
            results[workload] = samples

    stage_order = [
        "Source file read", "Lexing", "Parsing", "Import loading",
        "Semantic analysis", "Reachability analysis", "IR generation",
        "IR optimization", "Target code generation",
        "AST fallback code generation", "Assembly output", "Native linking",
        "Program execution", "Total wall time", "Benchmark process wall time",
    ]
    print("Swan compiler pipeline benchmark")
    print(f"Compiler: {compiler}")
    print(f"Statistic: median of {args.repeats} measured runs; "
          f"{args.warmups} warm-up run(s) per workload")
    print("Unit: seconds (lower is faster)")
    print("IR generation includes SIL bridging and typed-IR fallback; "
          "they are not separately timed by the current profiler.")
    for workload, samples in results.items():
        print(f"\n[{workload}]")
        keys = set().union(*(sample.keys() for sample in samples))
        for stage in stage_order:
            if stage not in keys:
                continue
            median = statistics.median(sample.get(stage, 0.0) for sample in samples)
            print(f"{stage:34s} {median:.9f} seconds")


if __name__ == "__main__":
    main()
