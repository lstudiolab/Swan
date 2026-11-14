#!/usr/bin/env python3
"""Reproducible 1B-element heapsort comparison for Swan, Goose, Rust, C, C++, Swift."""
from __future__ import annotations

import json
import os
import pathlib
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[1]
BENCH = ROOT / "benchmarks" / "heapsort"
COUNT = 1_000_000_000
ROUNDS = 3
SWAN = pathlib.Path(os.environ.get("SWAN", ROOT / "build" / "swanc"))
GOOSE = pathlib.Path(os.environ.get("GOOSE", ROOT.parent / "goose" / "build" / "goose"))
NAMES = ["Swan", "Goose", "Rust", "C", "C++", "Swift"]
REPORT = ROOT / "heapsort-benchmark.json"


def command_output(command: list[str]) -> str:
    p = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, check=True)
    return p.stdout.strip()


def timed(command: list[str]) -> tuple[float, float, float, str]:
    with tempfile.NamedTemporaryFile(mode="w+", delete=False) as f:
        timing_path = pathlib.Path(f.name)
    try:
        p = subprocess.run(
            ["/usr/bin/time", "-f", "%e %U %S %M", "-o", str(timing_path), *command],
            cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        fields = timing_path.read_text().strip().split()
        if p.returncode:
            raise RuntimeError(
                f"command failed ({p.returncode}): {' '.join(command)}\n"
                f"stdout:\n{p.stdout}\nstderr:\n{p.stderr}"
            )
        if len(fields) != 4:
            raise RuntimeError(f"unexpected /usr/bin/time output: {fields!r}")
        wall, user, system, peak_kib = map(float, fields)
        return wall, user + system, peak_kib / 1024.0, p.stdout.strip()
    finally:
        timing_path.unlink(missing_ok=True)


def checksum_from(output: str) -> int:
    matches = re.findall(r"(?m)^\s*(\d+)\s*$", output)
    if not matches:
        raise RuntimeError(f"program did not print a numeric checksum: {output[-2000:]!r}")
    return int(matches[-1])


def save_report(report: dict) -> None:
    REPORT.write_text(json.dumps(report, indent=2) + "\n")


def main() -> int:
    if not SWAN.is_file():
        raise RuntimeError(f"Swan compiler not found: {SWAN}")
    if not GOOSE.is_file():
        raise RuntimeError(f"Goose compiler not found: {GOOSE}")

    with tempfile.TemporaryDirectory(prefix="heapsort-1b-") as temp:
        out = pathlib.Path(temp)
        binaries = {
            "Swan": out / "swan",
            "Goose": out / "goose",
            "Rust": out / "rust",
            "C": out / "c",
            "C++": out / "cpp",
            "Swift": out / "swift",
        }
        sources = {
            "Swan": BENCH / "swan.swan",
            "Goose": BENCH / "goose.goose",
            "Rust": BENCH / "rust.rs",
            "C": BENCH / "c.c",
            "C++": BENCH / "cpp.source",
            "Swift": BENCH / "swift.swift",
        }
        compile_commands = {
            "Swan": [str(SWAN), "build", str(sources["Swan"]), "-o", str(binaries["Swan"])],
            "Goose": [str(GOOSE), "build", "--stack-reserve", "8G", "-o",
                      str(binaries["Goose"]), str(sources["Goose"])],
            "Rust": ["rustc", "-C", "opt-level=3", "-C", "target-cpu=native",
                     str(sources["Rust"]), "-o", str(binaries["Rust"])],
            "C": ["cc", "-O3", "-march=native", "-DNDEBUG",
                  str(sources["C"]), "-o", str(binaries["C"])],
            "C++": ["c++", "-O3", "-march=native", "-DNDEBUG", "-std=c++20", "-x", "c++",
                    str(sources["C++"]), "-o", str(binaries["C++"])],
            "Swift": ["swiftc", "-O", "-whole-module-optimization",
                      str(sources["Swift"]), "-o", str(binaries["Swift"])],
        }

        report = {
            "benchmark": "in-place max-heapsort",
            "elements": COUNT,
            "element_type": "u32",
            "input": {
                "generator": "lcg32",
                "seed_decimal": 305419896,
                "sequence": "state = (state * 1664525 + 1013904223) % 4294967296",
                "same_sequence_in_all_implementations": True,
            },
            "verification": "monotonic nondecreasing order plus u64 sum checksum",
            "measurement_rounds": ROUNDS,
            "measurement_order": [],
            "runner": {
                "os": platform.platform(),
                "machine": platform.machine(),
                "processor": platform.processor(),
                "python": sys.version,
                "utc_started": datetime.now(timezone.utc).isoformat(),
            },
            "toolchains": {},
            "languages": {},
            "all_checksums_match": False,
        }
        for name, command in {
            "Swan": [str(SWAN), "--version"],
            "Goose": [str(GOOSE), "version"],
            "Rust": ["rustc", "--version"],
            "C": ["cc", "--version"],
            "C++": ["c++", "--version"],
            "Swift": ["swiftc", "--version"],
        }.items():
            try:
                report["toolchains"][name] = command_output(command).splitlines()[0]
            except Exception as error:
                report["toolchains"][name] = f"unavailable: {error}"

        save_report(report)
        results: dict[str, dict] = {}
        for name in NAMES:
            print(f"\n=== COMPILE {name} ===", flush=True)
            wall, cpu, peak, output = timed(compile_commands[name])
            if not binaries[name].is_file():
                raise RuntimeError(f"{name} compiler did not produce {binaries[name]}\n{output}")
            results[name] = {
                "compile_wall_seconds": round(wall, 3),
                "compile_cpu_seconds": round(cpu, 3),
                "compile_peak_rss_mib": round(peak, 2),
                "executable_bytes": binaries[name].stat().st_size,
                "runtime_samples_wall_seconds": [],
                "runtime_samples_cpu_seconds": [],
                "runtime_samples_peak_rss_mib": [],
            }
            report["languages"] = results
            save_report(report)
            print(f"COMPILED {name}: {wall:.3f}s, {binaries[name].stat().st_size} bytes", flush=True)

        expected_checksum: int | None = None
        for round_index in range(ROUNDS):
            order = NAMES[round_index:] + NAMES[:round_index]
            report["measurement_order"].append(order)
            print(f"\n=== RUNTIME ROUND {round_index + 1}/{ROUNDS}: {', '.join(order)} ===",
                  flush=True)
            for name in order:
                wall, cpu, peak, output = timed([str(binaries[name])])
                checksum = checksum_from(output)
                if expected_checksum is None:
                    expected_checksum = checksum
                elif checksum != expected_checksum:
                    raise RuntimeError(
                        f"{name} checksum mismatch: {checksum}; expected {expected_checksum}"
                    )
                results[name]["runtime_samples_wall_seconds"].append(round(wall, 3))
                results[name]["runtime_samples_cpu_seconds"].append(round(cpu, 3))
                results[name]["runtime_samples_peak_rss_mib"].append(round(peak, 2))
                results[name]["checksum"] = checksum
                results[name]["sortedness_verified"] = True
                results[name]["runtime_wall_median_seconds"] = round(
                    statistics.median(results[name]["runtime_samples_wall_seconds"]), 3
                )
                results[name]["runtime_cpu_median_seconds"] = round(
                    statistics.median(results[name]["runtime_samples_cpu_seconds"]), 3
                )
                results[name]["peak_rss_median_mib"] = round(
                    statistics.median(results[name]["runtime_samples_peak_rss_mib"]), 2
                )
                report["languages"] = results
                report["expected_checksum"] = expected_checksum
                save_report(report)
                print(
                    f"PASS {name}: wall={wall:.3f}s CPU={cpu:.3f}s "
                    f"RSS={peak:.2f} MiB checksum={checksum}",
                    flush=True,
                )

        report["expected_checksum"] = expected_checksum
        report["all_checksums_match"] = True
        report["utc_finished"] = datetime.now(timezone.utc).isoformat()
        report["ranking_by_runtime_median"] = sorted(
            NAMES, key=lambda name: results[name]["runtime_wall_median_seconds"]
        )
        save_report(report)

        print("\n| Language | Compile s | Runtime median s | CPU median s | Peak RSS MiB | Binary bytes |")
        print("|---|---:|---:|---:|---:|---:|")
        for name in sorted(NAMES, key=lambda item: results[item]["runtime_wall_median_seconds"]):
            item = results[name]
            print(
                f"| {name} | {item['compile_wall_seconds']:.3f} | "
                f"{item['runtime_wall_median_seconds']:.3f} | "
                f"{item['runtime_cpu_median_seconds']:.3f} | "
                f"{item['peak_rss_median_mib']:.2f} | {item['executable_bytes']} |"
            )
        print(f"\nAll six implementations verified checksum: {expected_checksum}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"BENCHMARK FAILED: {error}", file=sys.stderr, flush=True)
        raise
