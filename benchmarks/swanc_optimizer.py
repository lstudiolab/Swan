#!/usr/bin/env python3
"""Benchmark the executable Compiler 2 linear-IR optimization slice.

This deliberately labels the execution model: Compiler 1 bootstraps and runs
the Swan-authored Compiler 2 optimizer source. It is not a full swanc compiler
benchmark because Compiler 2's driver and later pipeline stages are not wired.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import platform
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
INSTRUCTIONS = 3072
DEFAULT_COMPILER = ROOT / "build" / "swan"


def make_source(destination: pathlib.Path) -> pathlib.Path:
    parts = [
        ROOT / "swanc/ir/IR.swan",
        ROOT / "swanc/optimization/ConstantFold.swan",
        ROOT / "swanc/optimization/Optimization.swan",
        ROOT / "tests/swanc_optimizer.swan",
    ]
    destination.write_text(
        "\n".join(path.read_text(encoding="utf-8") for path in parts),
        encoding="utf-8",
    )
    return destination


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=pathlib.Path, default=DEFAULT_COMPILER)
    parser.add_argument("--repeats", type=int, default=9)
    parser.add_argument("--output-dir", type=pathlib.Path,
                        default=ROOT / "benchmark-results")
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("--repeats must be at least 1")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}; run sh build.sh first.")

    with tempfile.TemporaryDirectory(prefix="swanc-optimizer-") as temporary:
        directory = pathlib.Path(temporary)
        source = make_source(directory / "optimizer.swan")
        executable = directory / "optimizer-test"
        build_start = time.perf_counter()
        built = subprocess.run(
            [str(compiler), "build", str(source), "-o", str(executable)],
            cwd=ROOT, text=True, capture_output=True,
        )
        build_seconds = time.perf_counter() - build_start
        if built.returncode:
            raise RuntimeError(f"Compiler 2 optimizer harness failed to build:\n{built.stderr[-12000:]}")
        samples = []
        for _ in range(args.repeats):
            started = time.perf_counter()
            run = subprocess.run([str(executable)], cwd=ROOT, check=False,
                                 text=True, capture_output=True)
            elapsed = time.perf_counter() - started
            if run.returncode:
                raise RuntimeError(
                    f"Optimizer correctness harness failed ({run.returncode}):\n{run.stderr}"
                )
            samples.append(elapsed)

    median = statistics.median(samples)
    result = {
        "host": platform.platform(),
        "machine": platform.machine(),
        "compiler_generation": "Compiler 1 bootstrap executing Swan-authored Compiler 2 optimizer",
        "scope": "linear IR verification plus i32 constant folding; not full swanc",
        "instruction_count": INSTRUCTIONS,
        "repeats": args.repeats,
        "build_seconds": build_seconds,
        "runtime_samples_seconds": samples,
        "runtime_median_seconds": median,
        "instructions_per_second": INSTRUCTIONS / median if median else None,
    }
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "swanc-optimizer.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    report = [
        "# swanc optimizer microbenchmark", "",
        f"- Host: {result['host']} ({result['machine']})",
        f"- Execution: {result['compiler_generation']}",
        f"- Scope: {result['scope']}",
        f"- IR instructions: {INSTRUCTIONS}",
        f"- Harness build time: {build_seconds:.9f} s",
        f"- Median process runtime: {median:.9f} s",
        f"- Throughput: {result['instructions_per_second']:.2f} instructions/s",
        f"- Samples: {', '.join(f'{sample:.9f}' for sample in samples)}",
        "",
        "This result measures the implemented Compiler 2 optimizer slice only. "
        "It must not be presented as whole-compiler or self-hosted swanc performance.",
    ]
    (output / "swanc-optimizer.md").write_text("\n".join(report) + "\n",
                                                 encoding="utf-8")
    print("\n".join(report))
    print(f"JSON report: {output / 'swanc-optimizer.json'}")


if __name__ == "__main__":
    main()
