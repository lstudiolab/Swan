#!/usr/bin/env python3
"""Profile Swan's compiler files, pipeline stages, compile time, and runtime.

Run after: sh build.sh
  python3 benchmarks/compiler_pipeline.py --repeats 7 --warmups 2

The compiler's --profile timings measure Swan source processing. Clang's
-ftime-trace attributes C frontend work to individual included compiler files.
All durations are seconds. Reports are written as JSON, CSV, and Markdown.
"""
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import platform
import re
import shutil
import statistics
import subprocess
import tempfile
import time
from collections import defaultdict

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
    "Total wall time", "Benchmark process wall time", "Program runtime",
)


def write_source(directory: pathlib.Path, name: str, lines: list[str]) -> pathlib.Path:
    path = directory / f"{name}.swan"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def workloads(directory: pathlib.Path) -> dict[str, pathlib.Path]:
    result: dict[str, pathlib.Path] = {}
    result["control-flow"] = ROOT / "benchmarks" / "compiler_pipeline.swan"

    # Many declarations and identifiers; all helpers remain reachable.
    for count in (256, 512):
        lines: list[str] = []
        for i in range(count):
            lines += [f"fn front_{i}(value: int) -> int:",
                      f"    return ((value + {i + 3}) * 3 - {i % 17}) / 3", ""]
        lines += ["fn main() -> int:", "    var value = 7",
                  f"    value = front_{count - 1}(value)", "    return value"]
        result[f"frontend-{count}"] = write_source(directory, f"frontend-{count}", lines)

    # Deep dependency chain stresses name lookup and reachability.
    count = 512
    lines = ["fn chain_0(value: int) -> int:", "    return value + 1", ""]
    for i in range(1, count):
        lines += [f"fn chain_{i}(value: int) -> int:",
                  f"    return chain_{i - 1}(value) + {i % 101 + 1}", ""]
    lines += ["fn main() -> int:", f"    return chain_{count - 1}(1)"]
    result["deep-call-chain-512"] = write_source(directory, "deep-call-chain-512", lines)

    # Broad call graph with expression-tree pressure.
    count = 256
    lines = []
    for i in range(count):
        terms = " + ".join(f"(value * {j + 2} + {i + j})" for j in range(12))
        lines += [f"fn expr_{i}(value: int) -> int:", f"    return {terms}", ""]
    lines += ["fn main() -> int:", "    var total = 0"]
    lines += [f"    total = total + expr_{i}({i + 1})" for i in range(count)]
    lines += ["    return total"]
    result["expression-callgraph-256"] = write_source(directory, "expression-callgraph-256", lines)

    # Runtime workload: enough loop work to rise above process-start noise.
    result["runtime-loop"] = write_source(directory, "runtime-loop", [
        "fn main() -> int:",
        "    var i = 0",
        "    var total = 0",
        "    while i < 2000000:",
        "        total = total + (i % 97)",
        "        i = i + 1",
        "    if total != 95998839:",
        "        return 1",
        "    return 0",
    ])
    return result


def parse_profile(stderr: str) -> dict[str, float]:
    if HEADER not in stderr:
        raise RuntimeError("Compiler did not emit --profile output.")
    values: dict[str, float] = {}
    for line in stderr.splitlines():
        match = PROFILE_LINE.match(line)
        if match:
            values[match.group(1).strip()] = float(match.group(2))
    if "Total wall time" not in values:
        raise RuntimeError("Profile report omitted Total wall time.")
    return values


def compile_once(compiler: pathlib.Path, source: pathlib.Path,
                 output: pathlib.Path) -> dict[str, float]:
    start = time.perf_counter()
    proc = subprocess.run(
        [str(compiler), "build", "--profile", str(source), "-o", str(output)],
        cwd=ROOT, text=True, capture_output=True,
    )
    elapsed = time.perf_counter() - start
    if proc.returncode:
        raise RuntimeError(f"{source.name} compile failed:\n{proc.stderr[-12000:]}")
    if not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError(f"{source.name}: compiler produced no executable")
    values = parse_profile(proc.stderr)
    if source.stem == "runtime-loop" and values.get("AST fallback code generation", 0.0) > 0.0:
        raise RuntimeError("runtime-loop must exercise the optimized typed-IR native backend")
    values["Benchmark process wall time"] = elapsed
    run_start = time.perf_counter()
    executed = subprocess.run([str(output)], cwd=ROOT, text=True, capture_output=True,
                              check=False)
    values["Program runtime"] = time.perf_counter() - run_start
    if executed.returncode < 0:
        raise RuntimeError(f"{source.name}: program terminated by signal {-executed.returncode}")
    if source.stem == "runtime-loop":
        if executed.returncode != 0:
            raise RuntimeError(f"runtime-loop: expected exit code 0, got {executed.returncode}")
    return values


def profile_compiler_files(directory: pathlib.Path) -> list[dict[str, object]]:
    clang = shutil.which("clang")
    if not clang:
        print("WARNING: clang unavailable; per-file C/header trace skipped.")
        return []
    trace_dir = directory / "clang-trace"
    trace_dir.mkdir()
    command = [
        clang, "-std=c11", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-Wpedantic",
        "-DSWAN_SIL_BOOTSTRAP", f"-I{ROOT / 'compiler' / 'include'}",
        "-ftime-trace", "-c", str(ROOT / "main.c"), "-o", str(trace_dir / "main.o"),
    ]
    start = time.perf_counter()
    proc = subprocess.run(command, cwd=trace_dir, text=True, capture_output=True)
    compile_elapsed = time.perf_counter() - start
    if proc.returncode:
        raise RuntimeError(f"Clang time-trace compile failed:\n{proc.stderr[-12000:]}")
    trace_files = list(trace_dir.glob("*.json"))
    if not trace_files:
        raise RuntimeError("Clang completed but did not emit its -ftime-trace JSON.")
    data = json.loads(trace_files[0].read_text(encoding="utf-8"))
    totals: dict[str, float] = defaultdict(float)
    for event in data.get("traceEvents", []):
        if event.get("name") != "Source" or not isinstance(event.get("dur"), (int, float)):
            continue
        detail = str(event.get("args", {}).get("detail", "")).replace("\\", "/")
        marker = "/compiler/"
        if marker not in detail:
            continue
        relative = "compiler/" + detail.split(marker, 1)[1].split(":", 1)[0]
        totals[relative] += float(event["dur"]) / 1_000_000.0
    rows = [{"file": name, "compile_seconds": seconds}
            for name, seconds in sorted(totals.items(), key=lambda item: item[1], reverse=True)]
    for row in rows:
        row["share_percent"] = (
            float(row["compile_seconds"]) / sum(float(x["compile_seconds"]) for x in rows) * 100
            if rows else 0.0
        )
    rows.insert(0, {"file": "main.c + compiler frontend total",
                    "compile_seconds": compile_elapsed, "share_percent": 100.0})
    return rows


def write_reports(results: dict[str, list[dict[str, float]]],
                  file_rows: list[dict[str, object]], output_dir: pathlib.Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    summary = {
        "host": platform.platform(),
        "unit": "seconds",
        "workloads": {},
        "compiler_files": file_rows,
    }
    for name, samples in results.items():
        summary["workloads"][name] = {
            stage: statistics.median(s[stage] for s in samples if stage in s)
            for stage in STAGES if any(stage in s for s in samples)
        }
    (output_dir / "compiler-pipeline.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    with (output_dir / "compiler-pipeline.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["workload", "metric", "seconds"])
        for name, metrics in summary["workloads"].items():
            writer.writerows([name, metric, f"{value:.9f}"] for metric, value in metrics.items())
        for row in file_rows:
            writer.writerow(["compiler-file", row["file"], f"{float(row['compile_seconds']):.9f}"])
    lines = [
        "# Swan compiler pipeline benchmark", "",
        f"- Host: {summary['host']}",
        "- Unit: seconds (lower is faster)",
        "- Statistic: median of measured runs",
        "",
        "## Compilation stages and runtime", "",
        "| Workload | Metric | Seconds |",
        "|---|---|---:|",
    ]
    for name, metrics in summary["workloads"].items():
        for metric, value in metrics.items():
            lines.append(f"| {name} | {metric} | {value:.9f} |")
    lines += ["", "## Compiler source-file frontend cost", "",
              "| File | Compile seconds | Share of traced file time |",
              "|---|---:|---:|"]
    for row in file_rows:
        lines.append(f"| {row['file']} | {float(row['compile_seconds']):.9f} | "
                     f"{float(row['share_percent']):.2f}% |")
    (output_dir / "compiler-pipeline.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=pathlib.Path, default=DEFAULT_COMPILER)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--output-dir", type=pathlib.Path, default=ROOT / "benchmark-results")
    args = parser.parse_args()
    if args.repeats < 1 or args.warmups < 0:
        parser.error("--repeats must be >= 1 and --warmups >= 0")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}; run sh build.sh first.")

    with tempfile.TemporaryDirectory(prefix="swan-pipeline-") as temp:
        temp_dir = pathlib.Path(temp)
        cases = workloads(temp_dir)
        results: dict[str, list[dict[str, float]]] = {}
        for name, source in cases.items():
            output = temp_dir / f"{name}.out"
            for _ in range(args.warmups):
                compile_once(compiler, source, output)
            results[name] = [compile_once(compiler, source, output)
                             for _ in range(args.repeats)]
        file_rows = profile_compiler_files(temp_dir)

    write_reports(results, file_rows, args.output_dir.resolve())
    print("Swan compiler pipeline benchmark")
    print(f"Host: {platform.platform()} / {platform.machine()}")
    print(f"Compiler: {compiler}")
    print(f"Median of {args.repeats} runs; {args.warmups} warmups; seconds")
    for name, samples in results.items():
        print(f"\n[{name}]")
        for stage in STAGES:
            if any(stage in sample for sample in samples):
                value = statistics.median(sample[stage] for sample in samples if stage in sample)
                print(f"{stage:34s} {value:.9f} seconds")
    print("\n[slowest compiler files by Clang frontend trace]")
    for row in file_rows[1:11]:
        print(f"{row['file']:52s} {float(row['compile_seconds']):.9f} seconds "
              f"({float(row['share_percent']):.2f}%)")
    print(f"\nReports: {args.output_dir.resolve()}/compiler-pipeline.{{md,csv,json}}")


if __name__ == "__main__":
    main()
