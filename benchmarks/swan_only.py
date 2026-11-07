#!/usr/bin/env python3
"""Transparent Swan-only benchmark with wall/CPU time and peak-RSS measurements."""
from __future__ import annotations

import json
import os
import pathlib
import platform
import re
import shutil
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
ITERATIONS = 1_000_000_000
COMPILE_REPEATS = 5
RUNTIME_REPEATS = 5
TIME_BIN = "/usr/bin/time"


def expected_checksum(n: int) -> int:
    counts = [n // 97 + (r < n % 97) for r in range(97)]
    return sum(
        counts[i] * counts[j] * counts[k] * ((31*i + 17*j + 13*k) % 97)
        for i in range(97) for j in range(97) for k in range(97)
    )


def machine_info() -> dict:
    cpu_model = "unknown"
    try:
        for line in pathlib.Path("/proc/cpuinfo").read_text().splitlines():
            if line.lower().startswith("model name"):
                cpu_model = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    mem = {}
    try:
        for line in pathlib.Path("/proc/meminfo").read_text().splitlines():
            key, value = line.split(":", 1)
            if key in ("MemTotal", "MemAvailable"):
                mem[key] = value.strip()
    except (OSError, ValueError):
        pass
    return {
        "os": platform.platform(),
        "kernel": platform.release(),
        "architecture": platform.machine(),
        "cpu_model": cpu_model,
        "logical_cpu_count": os.cpu_count(),
        "memory": mem,
        "python": platform.python_version(),
        "cc_path": shutil.which("cc"),
        "time_utility": TIME_BIN if pathlib.Path(TIME_BIN).exists() else None,
        "github_actions": os.environ.get("GITHUB_ACTIONS", "false"),
        "runner_os": os.environ.get("RUNNER_OS"),
        "runner_arch": os.environ.get("RUNNER_ARCH"),
    }


def measured(command: list[str], expected: str | None = None) -> tuple[dict, str]:
    if not pathlib.Path(TIME_BIN).exists():
        raise RuntimeError("GNU /usr/bin/time is required for transparent resource measurements")
    fmt = "SWAN_TIME_JSON:%e,%U,%S,%M,%P,%F,%R,%w,%c"
    wall_start = time.perf_counter()
    result = subprocess.run(
        [TIME_BIN, "-f", fmt, *command],
        cwd=ROOT, text=True, capture_output=True,
    )
    high_resolution_wall = time.perf_counter() - wall_start
    match = re.search(r"SWAN_TIME_JSON:([0-9.]+),([0-9.]+),([0-9.]+),([0-9]+),([^,]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+)", result.stderr)
    if result.returncode:
        raise RuntimeError(f"{command} failed ({result.returncode}):\n{result.stderr}")
    if not match:
        raise RuntimeError(f"Could not parse GNU time metrics:\n{result.stderr}")
    wall, user, system, rss_kb, cpu_pct, major_pf, minor_pf, voluntary, involuntary = match.groups()
    output = result.stdout.strip()
    if expected is not None and output != expected:
        raise RuntimeError(f"wrong result: expected {expected}, got {output!r}")
    metrics = {
        "wall_seconds": high_resolution_wall,
        "gnu_time_wall_seconds_rounded": float(wall),
        "user_cpu_seconds": float(user),
        "system_cpu_seconds": float(system),
        "total_cpu_seconds": float(user) + float(system),
        "peak_rss_kib": int(rss_kb),
        "peak_rss_mib": int(rss_kb) / 1024,
        "cpu_utilization_percent": cpu_pct,
        "major_page_faults": int(major_pf),
        "minor_page_faults": int(minor_pf),
        "voluntary_context_switches": int(voluntary),
        "involuntary_context_switches": int(involuntary),
    }
    return metrics, output


def median_metric(samples: list[dict], key: str):
    return statistics.median(sample[key] for sample in samples)


def main() -> None:
    host = machine_info()
    build, _ = measured(["sh", str(ROOT / "build.sh")])
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
        for _ in range(COMPILE_REPEATS):
            metrics, _ = measured([str(SWAN), str(source), "--emit-asm", "-o", str(assembly)])
            compile_samples.append(metrics)
        assemble_samples = []
        for _ in range(3):
            metrics, _ = measured(["cc", "-x", "assembler", str(assembly), "-o", str(executable)])
            assemble_samples.append(metrics)

        correctness_metrics, output = measured([str(executable)], expected)
        runtime_samples = []
        for index in range(RUNTIME_REPEATS):
            metrics, _ = measured([str(executable)], expected)
            runtime_samples.append(metrics)
            print(f"RUN {index + 1}/{RUNTIME_REPEATS}: {metrics['wall_seconds']:.6f}s "
                  f"wall, {metrics['total_cpu_seconds']:.6f}s CPU, "
                  f"{metrics['peak_rss_mib']:.3f} MiB peak RSS", flush=True)

        shutil.copyfile(assembly, ROOT / "swan-only-benchmark.s")

        report = {
            "benchmark": "Swan-only one-billion nested-loop stress test",
            "commit": os.environ.get("GITHUB_SHA"),
            "host": host,
            "compiler": {
                "implementation": "Swan C11 frontend + Swan-owned x86-64 System V assembly backend",
                "compiler_binary_bytes": SWAN.stat().st_size,
                "build_measurement": build,
                "compile_samples": compile_samples,
                "compile_wall_seconds_median": median_metric(compile_samples, "wall_seconds"),
                "compile_cpu_seconds_median": median_metric(compile_samples, "total_cpu_seconds"),
                "compile_peak_rss_mib_median": median_metric(compile_samples, "peak_rss_mib"),
                "assembly_bytes": assembly.stat().st_size,
            },
            "assembler_linker": {
                "command": "cc -x assembler (host assembler/linker; not a Swan compiler backend)",
                "samples": assemble_samples,
                "wall_seconds_median": median_metric(assemble_samples, "wall_seconds"),
                "cpu_seconds_median": median_metric(assemble_samples, "total_cpu_seconds"),
                "peak_rss_mib_median": median_metric(assemble_samples, "peak_rss_mib"),
                "executable_bytes": executable.stat().st_size,
            },
            "workload": {
                "description": "three nested loops, 1000 x 1000 x 1000 iterations",
                "iterations": ITERATIONS,
                "source_bytes": source.stat().st_size,
                "expected_checksum": expected,
                "actual_checksum": output,
                "correctness": "PASS",
            },
            "runtime": {
                "samples": runtime_samples,
                "wall_seconds_median": median_metric(runtime_samples, "wall_seconds"),
                "wall_seconds_min": min(s["wall_seconds"] for s in runtime_samples),
                "wall_seconds_max": max(s["wall_seconds"] for s in runtime_samples),
                "cpu_seconds_median": median_metric(runtime_samples, "total_cpu_seconds"),
                "user_cpu_seconds_median": median_metric(runtime_samples, "user_cpu_seconds"),
                "system_cpu_seconds_median": median_metric(runtime_samples, "system_cpu_seconds"),
                "peak_rss_mib_median": median_metric(runtime_samples, "peak_rss_mib"),
                "peak_rss_mib_max": max(s["peak_rss_mib"] for s in runtime_samples),
                "cpu_utilization_percent_median": statistics.median(
                    float(s["cpu_utilization_percent"].rstrip("%")) for s in runtime_samples
                ),
                "iterations_per_second": ITERATIONS / median_metric(runtime_samples, "wall_seconds"),
            },
            "measurement_notes": [
                "Wall time, user CPU time, system CPU time and peak resident memory are measured per process with GNU time.",
                "Compilation, assembly/linking and execution are timed separately.",
                "Build time is reported separately and excluded from per-source compile time. Python perf_counter supplies high-resolution wall time; GNU time reports CPU times rounded to centiseconds on this runner.",
                "Peak RSS is process maximum resident set size, not total machine memory consumption.",
                "The benchmark uses a GitHub-hosted runner; CPU model and RAM availability are recorded because hosted hardware can vary.",
                "No other language is compiled or run in this benchmark.",
            ],
        }
        out = ROOT / "swan-only-benchmark.json"
        out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
