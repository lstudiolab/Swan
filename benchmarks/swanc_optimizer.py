#!/usr/bin/env python3
"""Benchmark Swan-authored Compiler 2 optimizer code emitted by Compiler 1.

Measures only the implemented linear-IR verifier + i32 constant-folding
pipeline. It is not a whole-compiler or self-hosted swanc benchmark.
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
INSTRUCTIONS_PER_RUN = 384 * 128
FOLDS_PER_RUN = 128 * 128
DEFAULT_COMPILER = ROOT / "build" / "swan"


def make_source(destination: pathlib.Path) -> pathlib.Path:
    parts = [
        ROOT / "swanc/ir/IR.swan",
        ROOT / "swanc/optimization/ConstantFold.swan",
        ROOT / "swanc/optimization/Optimization.swan",
    ]
    source = "\n".join(p.read_text(encoding="utf-8") for p in parts)
    source += "fn main() -> int:\n    return swanc_ir_optimize(array_unchecked(8), 0, array_unchecked(2), 1)\nout\n"
    destination.write_text(source, encoding="utf-8")
    return destination


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=pathlib.Path, default=DEFAULT_COMPILER)
    parser.add_argument("--repeats", type=int, default=9)
    parser.add_argument("--output-dir", type=pathlib.Path, default=ROOT / "benchmark-results")
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("--repeats must be at least 1")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}; run sh build.sh first.")

    with tempfile.TemporaryDirectory(prefix="swanc-optimizer-") as temporary:
        directory = pathlib.Path(temporary)
        source = make_source(directory / "optimizer.swan")
        assembly = directory / "optimizer.s"
        executable = directory / "optimizer-test"
        started = time.perf_counter()
        emitted = subprocess.run(
            [str(compiler), str(source), "--target", "x86_64", "--emit-asm", "-o", str(assembly)],
            cwd=ROOT, text=True, capture_output=True,
        )
        if emitted.returncode:
            raise RuntimeError(f"Compiler 2 optimizer module failed to compile:\n{emitted.stderr[-12000:]}")
        rename = subprocess.run(
            ["sed", "-i", "-E",
             r"s/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_optimizer_module_stub\2/g",
             str(assembly)], cwd=ROOT, text=True, capture_output=True,
        )
        if rename.returncode:
            raise RuntimeError(f"Could not rename generated module entry point:\n{rename.stderr}")
        subprocess.run(
            ["sed", "-i", "-E", r"s/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_optimizer_module_stub\2/g", str(assembly)],
            cwd=ROOT, check=True,
        )
        linked = subprocess.run(
            ["cc", "-O2", str(assembly), str(ROOT / "tests/swanc_optimizer.c"), "-o", str(executable)],
            cwd=ROOT, text=True, capture_output=True,
        )
        build_seconds = time.perf_counter() - started
        if linked.returncode:
            raise RuntimeError(f"Optimizer benchmark harness failed to link:\n{linked.stderr[-12000:]}")
        check = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True)
        if check.returncode:
            raise RuntimeError(f"Optimizer correctness check failed:\n{check.stderr}")
        samples = []
        for _ in range(args.repeats):
            started = time.perf_counter()
            run = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True)
            elapsed = time.perf_counter() - started
            if run.returncode:
                raise RuntimeError(f"Optimizer benchmark run failed:\n{run.stderr}")
            samples.append(elapsed)

    median = statistics.median(samples)
    result = {
        "host": platform.platform(),
        "machine": platform.machine(),
        "compiler_generation": "Compiler 1 bootstrapping Swan-authored Compiler 2 optimizer",
        "scope": "linear IR verification plus i32 constant folding; not full swanc",
        "ir_instructions_per_run": INSTRUCTIONS_PER_RUN,
        "constant_folds_per_run": FOLDS_PER_RUN,
        "repeats": args.repeats,
        "module_compile_and_link_seconds": build_seconds,
        "runtime_samples_seconds": samples,
        "runtime_median_seconds": median,
        "ir_instructions_per_second": INSTRUCTIONS_PER_RUN / median if median else None,
        "constant_folds_per_second": FOLDS_PER_RUN / median if median else None,
    }
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "swanc-optimizer.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    report = [
        "# swanc optimizer microbenchmark", "",
        f"- Host: {result['host']} ({result['machine']})",
        f"- Execution: {result['compiler_generation']}",
        f"- Scope: {result['scope']}",
        f"- IR instructions per run: {INSTRUCTIONS_PER_RUN:,}",
        f"- Constant folds per run: {FOLDS_PER_RUN:,}",
        f"- Module compile + link: {build_seconds:.9f} s",
        f"- Median runtime: {median:.9f} s",
        f"- IR throughput: {result['ir_instructions_per_second']:.2f} instructions/s",
        f"- Fold throughput: {result['constant_folds_per_second']:.2f} folds/s",
        f"- Samples: {', '.join(f'{x:.9f}' for x in samples)}",
        "",
        "This result measures the implemented Compiler 2 optimizer slice only; it is not whole-compiler or self-hosted swanc performance.",
    ]
    (output / "swanc-optimizer.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    print("\n".join(report))
    print(f"JSON report: {output / 'swanc-optimizer.json'}")


if __name__ == "__main__":
    main()
