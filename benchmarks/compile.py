#!/usr/bin/env python3
"""Repeatable end-to-end Swan compiler benchmark; reports timings, not speedup claims."""
from pathlib import Path
from statistics import median
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "swan"
REPEATS = 7
DECLARATIONS = 2500

if not BINARY.exists():
    subprocess.run(["sh", str(ROOT / "build.sh")], check=True)

with tempfile.TemporaryDirectory(prefix="swan-bench-") as directory:
    source = Path(directory) / "large.swan"
    output = Path(directory) / "large.c"
    lines = ["fn main() {"]
    lines.extend(f"    let value{i}: int = {i}" for i in range(DECLARATIONS))
    lines.append(f"    log value{DECLARATIONS - 1}")
    lines.append("}")
    source.write_text("\n".join(lines) + "\n", encoding="utf-8")

    samples = []
    for _ in range(REPEATS):
        start = time.perf_counter()
        subprocess.run([str(BINARY), str(source), "-o", str(output)],
                       check=True, stdout=subprocess.DEVNULL)
        samples.append(time.perf_counter() - start)

    print(f"fixture: {DECLARATIONS:,} declarations")
    print(f"runs: {REPEATS}")
    print(f"median: {median(samples) * 1000:.2f} ms")
    print("samples: " + ", ".join(f"{sample * 1000:.2f} ms" for sample in samples))
