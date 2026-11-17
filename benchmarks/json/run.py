#!/usr/bin/env python3
"""Equivalent JSON parse + compact-encode workload for Swan, Go, Goose and Rust."""
from __future__ import annotations

import json
import os
import pathlib
import platform
import shutil
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
BENCH = pathlib.Path(__file__).resolve().parent
REQUESTS = 100_000
REPEATS = 7
PAYLOAD = (
    '{"id":42,"workers":128,"enabled":true,"service":"swan",'
    '"items":[1,2,3,4,5],"meta":{"region":"ap-southeast-1","version":1}}'
)
EXPECTED_CHECKSUM = REQUESTS * (42 + 128)
EXPECTED_BYTES = REQUESTS * len(PAYLOAD.encode()) * 2


def run(command: list[str], cwd: pathlib.Path) -> tuple[float, tuple[int, int]]:
    start = time.perf_counter()
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    lines = result.stdout.strip().splitlines()
    if len(lines) != 2:
        raise RuntimeError(f"expected checksum and byte count, got: {result.stdout!r}")
    return elapsed, (int(lines[0]), int(lines[1]))


def version(command: list[str]) -> str:
    p = subprocess.run(command, text=True, capture_output=True)
    text = p.stdout or p.stderr
    return text.splitlines()[0] if p.returncode == 0 and text else "unavailable"


def median(values: list[float]) -> float:
    return statistics.median(values)


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int((len(ordered) - 1) * fraction + 0.5)))
    return ordered[index]


def host() -> dict:
    cpu = "unknown"
    try:
        for line in pathlib.Path("/proc/cpuinfo").read_text().splitlines():
            if line.lower().startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    return {
        "os": platform.platform(),
        "architecture": platform.machine(),
        "cpu": cpu,
        "logical_cpus": os.cpu_count(),
    }


def main() -> None:
    goose = os.environ.get("GOOSE", "goose")
    cc = shutil.which("cc")
    go = shutil.which("go")
    cargo = shutil.which("cargo")
    if not all((cc, go, cargo, shutil.which(goose))):
        raise SystemExit("Required tools: cc, go, cargo/rustc, and Goose (GOOSE or PATH)")

    versions = {
        "Swan": "repository compiler",
        "Go": version([go, "version"]),
        "Goose": version([goose, "version"]),
        "Rust": version(["rustc", "--version"]),
    }
    outputs: dict[str, pathlib.Path] = {}
    compile_seconds: dict[str, float] = {}
    flags: dict[str, str] = {}

    with tempfile.TemporaryDirectory(prefix="swan-json-bench-") as temp:
        tmp = pathlib.Path(temp)
        swan = ROOT / "build" / "swan"
        if not swan.exists():
            subprocess.run(["sh", "build.sh"], cwd=ROOT, check=True)

        # Swan uses the actual JSON runtime that its standard module calls.
        swan_obj = tmp / "json_abi.o"
        t0 = time.perf_counter()
        subprocess.run(
            [cc, "-O3", "-DNDEBUG", "-std=c11", "-I", str(ROOT / "runtime"),
             "-c", str(ROOT / "runtime" / "json_abi.c"), "-o", str(swan_obj)],
            cwd=ROOT, check=True, capture_output=True,
        )
        swan_bin = tmp / "swan-json"
        subprocess.run(
            [str(swan), "build", str(BENCH / "swan.swan"), "--link",
             str(swan_obj), "-o", str(swan_bin)],
            cwd=ROOT, check=True, capture_output=True,
        )
        compile_seconds["Swan"] = time.perf_counter() - t0
        outputs["Swan"] = swan_bin
        flags["Swan"] = "Swan x86-64 AST compatibility backend + C11 JSON runtime (-O3)"

        go_bin = tmp / "go-json"
        t0 = time.perf_counter()
        subprocess.run([go, "build", "-trimpath", "-o", str(go_bin),
                        str(BENCH / "main.go")], cwd=ROOT, check=True, capture_output=True)
        compile_seconds["Go"] = time.perf_counter() - t0
        outputs["Go"] = go_bin
        flags["Go"] = "go build (default release compiler settings)"

        goose_bin = tmp / "goose-json"
        t0 = time.perf_counter()
        subprocess.run([goose, "build", "-o", str(goose_bin),
                        str(BENCH / "goose.goose")], cwd=ROOT, check=True, capture_output=True)
        compile_seconds["Goose"] = time.perf_counter() - t0
        outputs["Goose"] = goose_bin
        flags["Goose"] = "goose build (default release compiler settings)"

        rust_target = tmp / "rust-target"
        t0 = time.perf_counter()
        subprocess.run(
            [cargo, "build", "--release", "--manifest-path", str(BENCH / "Cargo.toml"),
             "--target-dir", str(rust_target)],
            cwd=ROOT, check=True, capture_output=True,
        )
        compile_seconds["Rust"] = time.perf_counter() - t0
        outputs["Rust"] = rust_target / "release" / "swan-json-benchmark"
        flags["Rust"] = "cargo build --release; serde_json preserve_order"

        for name, binary in outputs.items():
            _, values = run([str(binary)], ROOT)
            if values != (EXPECTED_CHECKSUM, EXPECTED_BYTES):
                raise RuntimeError(
                    f"{name} correctness mismatch: {values}; expected "
                    f"{(EXPECTED_CHECKSUM, EXPECTED_BYTES)}"
                )

        samples: dict[str, list[dict]] = {name: [] for name in outputs}
        names = list(outputs)
        for round_no in range(REPEATS):
            order = names[round_no:] + names[:round_no]
            for name in order:
                elapsed, values = run([str(outputs[name])], ROOT)
                if values != (EXPECTED_CHECKSUM, EXPECTED_BYTES):
                    raise RuntimeError(f"{name} checksum changed during timed run: {values}")
                samples[name].append({
                    "wall_seconds": elapsed,
                    "requests_per_second": REQUESTS / elapsed,
                    "mib_per_second": EXPECTED_BYTES / elapsed / (1024 * 1024),
                })

        results = {}
        for name in names:
            xs = samples[name]
            rps = [x["requests_per_second"] for x in xs]
            mib = [x["mib_per_second"] for x in xs]
            results[name] = {
                "compiler_seconds": compile_seconds[name],
                "binary_bytes": outputs[name].stat().st_size,
                "settings": flags[name],
                "samples": xs,
                "median_requests_per_second": median(rps),
                "p95_run_requests_per_second": percentile(rps, 0.95),
                "p99_run_requests_per_second": percentile(rps, 0.99),
                "median_mib_per_second": median(mib),
            }

        report = {
            "benchmark": "JSON parse + compact encode: Swan / Go / Goose / Rust",
            "host": host(),
            "toolchains": versions,
            "workload": {
                "requests_per_run": REQUESTS,
                "repeats": REPEATS,
                "input_bytes_per_request": len(PAYLOAD.encode()),
                "expected_output_bytes_per_request": len(PAYLOAD.encode()),
                "bytes_per_request": len(PAYLOAD.encode()) * 2,
                "expected_checksum": EXPECTED_CHECKSUM,
                "expected_total_bytes": EXPECTED_BYTES,
                "correctness": "all implementations verified",
            },
            "languages": results,
            "methodology": [
                "Every implementation parses the same nested JSON request and compact-encodes the parsed document.",
                "Every run checks the same id/workers values and returns identical checksum and total input+output bytes.",
                "Swan calls its C11 JSON runtime through the new x86-64 C ABI; this is a Swan runtime benchmark, not a pure Swan parser.",
                "Go uses encoding/json; Goose uses its stdlib json DOM and writer; Rust uses serde_json with preserve_order.",
                "The benchmark is in-process and excludes TCP/HTTP, TLS, and socket overhead.",
                "Seven timed runs rotate execution order; throughput values are medians across runs.",
                "p95/p99 are percentiles across run-level throughput samples, not per-request latency percentiles.",
            ],
        }
        (ROOT / "json-runtime-benchmark.json").write_text(json.dumps(report, indent=2) + "\n")
        lines = [
            "# JSON parse + compact encode benchmark",
            "",
            f"Host: {report['host']['cpu']} ({report['host']['architecture']}); "
            f"{report['host']['logical_cpus']} logical CPUs.",
            f"Workload: {REQUESTS:,} requests/run; {REPEATS} timed runs; "
            f"{len(PAYLOAD.encode())} input bytes/request.",
            "",
            "| Language | Median requests/s | Median MiB/s | Compiler/build s | Binary bytes |",
            "|---|---:|---:|---:|---:|",
        ]
        for name in names:
            x = results[name]
            lines.append(
                f"| {name} | {x['median_requests_per_second']:.0f} | "
                f"{x['median_mib_per_second']:.2f} | {x['compiler_seconds']:.3f} | "
                f"{x['binary_bytes']} |"
            )
        lines += ["", "## Toolchains", ""]
        lines += [f"- **{name}:** {versions[name]}" for name in names]
        lines += ["", "## Backend settings", ""]
        lines += [f"- **{name}:** {flags[name]}" for name in names]
        lines += ["", "## Methodology", ""]
        lines += [f"- {item}" for item in report["methodology"]]
        (ROOT / "json-runtime-benchmark.md").write_text("\n".join(lines) + "\n")
        print("\n".join(lines))


if __name__ == "__main__":
    main()
