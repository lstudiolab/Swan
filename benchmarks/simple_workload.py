#!/usr/bin/env python3
"""Small identical integer-loop benchmark for Swan, Goose, Rust and C."""
import os
import pathlib
import platform
import shutil
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
N = 10_000_000
EXPECTED = 479_999_984
REPEATS = 5

SOURCES = {
    "Swan": f"""fn main() -> int:
    var total = 0
    var i = 0
    while i < {N}
        total = total + ((i * 13 + 7) % 97)
        i = i + 1
    log total
    return 0
""",
    "Goose": f"""import std;
fn main() {{
    var total: i64 = 0;
    var i: i64 = 0;
    while i < {N} {{
        total += (i * 13 + 7) % 97;
        i += 1;
    }}
    print(total);
}}
""",
    "Rust": f"""fn main() {{
    let mut total: i64 = 0;
    let mut i: i64 = 0;
    while i < {N} {{
        total += (i * 13 + 7) % 97;
        i += 1;
    }}
    println!("{{}}", std::hint::black_box(total));
}}
""",
    "C": f"""#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {{
    int64_t total = 0;
    for (int64_t i = 0; i < {N}; ++i)
        total += (i * 13 + 7) % 97;
    printf("%" PRId64 "\\n", total);
    return 0;
}}
""",
}


def timed(command, cwd):
    start = time.perf_counter()
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(f"Command failed: {' '.join(map(str, command))}\n{result.stderr}")
    return elapsed, result.stdout.strip()


def main():
    tools = {
        "Goose": os.environ.get("GOOSE", "goose"),
        "Rust": shutil.which("rustc"),
        "C": shutil.which("cc"),
    }
    if not tools["Rust"] or not tools["C"]:
        raise SystemExit("rustc and cc are required")
    if not SWAN.is_file():
        raise SystemExit("Build Swan first: sh build.sh")

    with tempfile.TemporaryDirectory(prefix="swan-simple-bench-") as td:
        work = pathlib.Path(td)
        paths = {}
        for name, source in SOURCES.items():
            ext = {"Swan": "swan", "Goose": "goose", "Rust": "rs", "C": "c"}[name]
            paths[name] = work / f"workload.{ext}"
            paths[name].write_text(source, encoding="utf-8")

        binaries = {}
        compile_times = {}
        for name in ("Swan", "Goose", "Rust", "C"):
            binary = work / name.lower()
            if name == "Swan":
                cmd = [str(SWAN), "build", str(paths[name]), "-o", str(binary)]
            elif name == "Goose":
                cmd = [tools[name], "build", "-o", str(binary), str(paths[name])]
            elif name == "Rust":
                cmd = [tools[name], "-C", "opt-level=3", "-C", "target-cpu=native",
                       str(paths[name]), "-o", str(binary)]
            else:
                cmd = [tools[name], "-O3", "-march=native", "-DNDEBUG",
                       str(paths[name]), "-o", str(binary)]
            compile_times[name], output = timed(cmd, ROOT)
            binaries[name] = binary
            _, actual = timed([str(binary)], ROOT)
            if actual != str(EXPECTED):
                raise RuntimeError(f"{name} checksum mismatch: expected {EXPECTED}, got {actual!r}")

        samples = {name: [] for name in binaries}
        order = list(binaries)
        for round_no in range(REPEATS):
            rotated = order[round_no:] + order[:round_no]
            for name in rotated:
                elapsed, output = timed([str(binaries[name])], ROOT)
                if output != str(EXPECTED):
                    raise RuntimeError(f"{name} checksum mismatch in run {round_no + 1}: {output!r}")
                samples[name].append(elapsed)

        print("Swan simple workload benchmark")
        print(f"Workload: {N:,} integer iterations; total += (i * 13 + 7) % 97")
        print(f"Expected checksum: {EXPECTED:,} (verified for every run)")
        print(f"Host: {platform.platform()} / {platform.machine()}")
        print(f"CPU: {platform.processor() or 'not reported'}")
        print(f"Samples per language: {REPEATS}; metric: median wall-clock seconds")
        print()
        print("| Language | Compile (s) | Runtime median (s) | Min (s) | Max (s) |")
        print("|---|---:|---:|---:|---:|")
        for name in order:
            values = samples[name]
            print(f"| {name} | {compile_times[name]:.6f} | "
                  f"{statistics.median(values):.6f} | {min(values):.6f} | {max(values):.6f} |")
        print()
        print("Runtime samples (seconds)")
        for name in order:
            print(f"{name}: " + ", ".join(f"{x:.6f}" for x in samples[name]))


if __name__ == "__main__":
    main()
