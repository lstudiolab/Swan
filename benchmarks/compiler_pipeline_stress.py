#!/usr/bin/env python3
"""Fresh stress benchmark for Swan's compiler pipeline.

Unlike compiler_pipeline.py, this benchmark scales generated programs and
stresses frontend throughput, name resolution, call-graph traversal, control
flow lowering, optimization, and native code generation.

Run after `sh build.sh`:
    python3 benchmarks/compiler_pipeline_stress.py
All reported durations are seconds. Results are medians across repeated runs.
"""
from __future__ import annotations

import argparse
import pathlib
import platform
import re
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_COMPILER = ROOT / "build" / "swan"
HEADER = "Swan compiler pipeline profile (seconds)"
PROFILE_LINE = re.compile(
    r"^\s{2}(.+?)\s+([0-9]+(?:\.[0-9]+)?) s(?:\s+\(\s*[0-9]+(?:\.[0-9]+)?%\))?\s*$"
)
STAGES = (
    "Source file read", "Lexing", "Parsing", "Import loading",
    "Semantic analysis", "Reachability analysis", "IR generation",
    "IR optimization", "Target code generation",
    "AST fallback code generation", "Assembly output", "Native linking",
    "Program execution", "Total wall time", "Benchmark process wall time",
)


def write_source(directory: pathlib.Path, name: str, lines: list[str]) -> pathlib.Path:
    path = directory / f"{name}.swan"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def workloads(directory: pathlib.Path) -> dict[str, pathlib.Path]:
    result: dict[str, pathlib.Path] = {}

    # Frontend scaling: thousands of declarations with varied identifiers,
    # literals, and expression trees. The call chain keeps all declarations live.
    for count in (256, 512):
        lines = []
        for i in range(count):
            lines.extend([
                f"fn front_{i}(value: int) -> int:",
                f"    return ((value + {i + 3}) * 3 - {i % 17}) / 3",
                "",
            ])
        lines.extend(["fn main() -> int:", "    var value = 7",
                      f"    value = front_{count - 1}(value)", "    return value"])
        result[f"frontend-{count}"] = write_source(
            directory, f"frontend-{count}", lines
        )

    # Deep dependency chain stresses symbol lookup, reachability and call
    # lowering without executing a deep recursion at runtime.
    count = 512
    lines = ["fn chain_0(value: int) -> int:", "    return value + 1", ""]
    for i in range(1, count):
        lines.extend([
            f"fn chain_{i}(value: int) -> int:",
            f"    return chain_{i - 1}(value) + {i % 101 + 1}",
            "",
        ])
    lines.extend(["fn main() -> int:", f"    return chain_{count - 1}(1)"])
    result["deep-call-chain-1024"] = write_source(
        directory, "deep-call-chain-1024", lines
    )

    # Broad call graph and expression pressure. main references every helper.
    count = 256
    lines = []
    for i in range(count):
        terms = " + ".join(f"(value * {j + 2} + {i + j})" for j in range(12))
        lines.extend([f"fn expr_{i}(value: int) -> int:",
                      f"    return {terms}", ""])
    lines.extend(["fn main() -> int:", "    var total = 0"])
    lines.extend(f"    total = total + expr_{i}({i + 1})" for i in range(count))
    lines.append("    return total")
    result["expression-callgraph-256"] = write_source(
        directory, "expression-callgraph-256", lines
    )

    # Control-flow-heavy program: branches, loops, comparisons, and calls.
    lines = []
    for i in range(64):
        lines.extend([
            f"fn flow_{i}(value: int) -> int:",
            "    var index = 0",
            "    var total = value",
            "    while index < 16:",
            f"        if (index + value) % 3 == 0:",
            f"            total = total + index + {i + 1}",
            "        else:",
            "            total = total + (index * 2)",
            "        index = index + 1",
            "    return total",
            "",
        ])
    lines.extend(["fn main() -> int:", "    var total = 0"])
    lines.extend(f"    total = total + flow_{i}({i})" for i in range(64))
    lines.append("    return total")
    result["control-flow-64"] = write_source(
        directory, "control-flow-64", lines
    )
    return result


def parse_profile(stderr: str) -> dict[str, float]:
    if HEADER not in stderr:
        raise RuntimeError(
            "Compiler did not emit the expected --profile report. "
            "Confirm this compiler build supports pipeline profiling."
        )
    parsed: dict[str, float] = {}
    for line in stderr.splitlines():
        match = PROFILE_LINE.match(line)
        if match:
            parsed[match.group(1).strip()] = float(match.group(2))
    if "Total wall time" not in parsed:
        raise RuntimeError("Profile report omitted Total wall time")
    return parsed


def compile_once(compiler: pathlib.Path, source: pathlib.Path,
                 output: pathlib.Path) -> dict[str, float]:
    command = [str(compiler), "build", "--profile", str(source), "-o", str(output)]
    start = time.perf_counter()
    proc = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    external = time.perf_counter() - start
    if proc.returncode:
        raise RuntimeError(
            f"{source.name} failed to compile (exit {proc.returncode}):\n"
            f"{proc.stderr[-12000:]}"
        )
    if not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError(f"{source.name}: compiler produced no executable")
    values = parse_profile(proc.stderr)
    values["Benchmark process wall time"] = external
    return values


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=pathlib.Path, default=DEFAULT_COMPILER)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--warmups", type=int, default=1)
    args = parser.parse_args()
    if args.repeats < 1 or args.warmups < 0:
        parser.error("--repeats must be positive and --warmups cannot be negative")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}. Run sh build.sh first.")

    with tempfile.TemporaryDirectory(prefix="swan-pipeline-stress-") as temp:
        directory = pathlib.Path(temp)
        cases = workloads(directory)
        results: dict[str, list[dict[str, float]]] = {}
        source_sizes: dict[str, int] = {}
        for name, source in cases.items():
            source_sizes[name] = source.stat().st_size
            output = directory / f"{name}.bin"
            for _ in range(args.warmups):
                compile_once(compiler, source, output)
            results[name] = [
                compile_once(compiler, source, output)
                for _ in range(args.repeats)
            ]

    print("Swan compiler pipeline stress benchmark")
    print(f"Host: {platform.platform()} / {platform.machine()}")
    print(f"Compiler: {compiler}")
    print(f"Statistic: median of {args.repeats} measured runs; "
          f"{args.warmups} warm-up run(s)")
    print("Unit: seconds (lower is faster)")
    print("Workloads are generated afresh for each benchmark invocation.")
    print("IR generation includes SIL bridging and typed-IR fallback.")
    for name, samples in results.items():
        print(f"\n[{name}] source={source_sizes[name]} bytes")
        keys = set().union(*(sample.keys() for sample in samples))
        for stage in STAGES:
            if stage in keys:
                median = statistics.median(s.get(stage, 0.0) for s in samples)
                print(f"{stage:34s} {median:.9f} seconds")


if __name__ == "__main__":
    main()
