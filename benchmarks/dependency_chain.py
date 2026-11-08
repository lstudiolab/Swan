#!/usr/bin/env python3
"""Fresh dependency-chain benchmark: identical 32-bit LCG workload across languages."""
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
N = 20_000_000
MASK = 0x7fffffff
SEED = 1
MULTIPLIER = 1664525
INCREMENT = 1013904223
REPEATS = 7


def expected_value():
    state = SEED
    for _ in range(N):
        state = (state * MULTIPLIER + INCREMENT) & MASK
    return state


SOURCES = {
    "Swan": f"""fn main() -> int:
    var state = {SEED}
    var i = 0
    while i < {N}
        state = (state * {MULTIPLIER} + {INCREMENT}) & {MASK}
        i = i + 1
    log state
    return 0
""",
    "Goose": f"""import std;
fn main() {{
    var state: i64 = {SEED};
    var i: i64 = 0;
    while i < {N} {{
        state = (state * {MULTIPLIER} + {INCREMENT}) & {MASK};
        i += 1;
    }}
    print(state);
}}
""",
    "Rust": f"""fn main() {{
    let mut state: i64 = {SEED};
    for _ in 0..{N} {{
        state = (state * {MULTIPLIER} + {INCREMENT}) & {MASK};
    }}
    println!("{{}}", std::hint::black_box(state));
}}
""",
    "C": f"""#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {{
    int64_t state = {SEED};
    for (int64_t i = 0; i < {N}; ++i)
        state = (state * {MULTIPLIER} + {INCREMENT}) & INT64_C({MASK});
    printf("%" PRId64 "\\n", state);
    return 0;
}}
""",
}


def timed(command, cwd):
    start = time.perf_counter()
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(
            f"Command failed: {' '.join(map(str, command))}\n{result.stderr}"
        )
    return elapsed, result.stdout.strip()


def main():
    tools = {
        "Goose": os.environ.get("GOOSE", "goose"),
        "Rust": shutil.which("rustc"),
        "C": shutil.which("cc"),
    }
    if not all(tools.values()):
        raise SystemExit("Goose, rustc and cc are required")
    if not SWAN.is_file():
        raise SystemExit("Build Swan first: sh build.sh")

    expected = str(expected_value())
    with tempfile.TemporaryDirectory(prefix="swan-dependency-bench-") as td:
        work = pathlib.Path(td)
        extensions = {"Swan": "swan", "Goose": "goose", "Rust": "rs", "C": "c"}
        sources = {}
        for name, source in SOURCES.items():
            sources[name] = work / f"dependency.{extensions[name]}"
            sources[name].write_text(source, encoding="utf-8")

        binaries = {}
        compile_times = {}
        for name in SOURCES:
            binary = work / name.lower()
            if name == "Swan":
                cmd = [str(SWAN), "build", str(sources[name]), "-o", str(binary)]
            elif name == "Goose":
                cmd = [tools[name], "build", "-o", str(binary), str(sources[name])]
            elif name == "Rust":
                cmd = [tools[name], "-C", "opt-level=3", "-C", "target-cpu=native",
                       str(sources[name]), "-o", str(binary)]
            else:
                cmd = [tools[name], "-O3", "-march=native", "-DNDEBUG",
                       str(sources[name]), "-o", str(binary)]
            compile_times[name], _ = timed(cmd, ROOT)
            binaries[name] = binary
            _, actual = timed([str(binary)], ROOT)
            if actual != expected:
                raise RuntimeError(
                    f"{name} checksum mismatch: expected {expected}, got {actual!r}"
                )

        samples = {name: [] for name in binaries}
        order = list(binaries)
        for round_no in range(REPEATS):
            rotated = order[round_no % len(order):] + order[:round_no % len(order)]
            for name in rotated:
                elapsed, actual = timed([str(binaries[name])], ROOT)
                if actual != expected:
                    raise RuntimeError(f"{name} checksum mismatch on sample {round_no + 1}")
                samples[name].append(elapsed)

        print("Swan dependency-chain benchmark")
        print(f"Workload: {N:,} dependent 32-bit LCG iterations")
        print(f"state = (state * {MULTIPLIER} + {INCREMENT}) & {MASK}")
        print(f"Verified final state: {expected}")
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
