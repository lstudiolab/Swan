#!/usr/bin/env python3
"""Benchmarks the active C-based Swan compiler; reports median wall time in seconds."""
import pathlib
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
CC = "cc"


def median(command, repeats, cwd, expected=None):
    values = []
    for _ in range(repeats):
        start = time.perf_counter()
        result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
        values.append(time.perf_counter() - start)
        if result.returncode:
            raise RuntimeError(f"{command} failed:\n{result.stderr}")
        if expected is not None and result.stdout.strip() != expected:
            raise RuntimeError(f"wrong output: {result.stdout!r}")
    return statistics.median(values)


def main():
    if not SWAN.is_file():
        raise SystemExit("Run sh build.sh first")
    with tempfile.TemporaryDirectory(prefix="swan-c-bench-") as temp:
        work = pathlib.Path(temp)
        source = work / "hello.swan"
        assembly = work / "hello.s"
        binary = work / "hello"
        source.write_text('fn main():\n    log "Swan C compiler benchmark"\n    return 0\n')
        expected = "Swan C compiler benchmark"

        emit = median([str(SWAN), str(source), "--target", "native", "--emit-asm",
                       "-o", str(assembly)], 31, work)
        link = median([CC, "-x", "assembler", str(assembly), "-o", str(binary)],
                      21, work)
        compile_run = median([str(SWAN), str(source), "--target", "native",
                                  "-o", str(binary)], 21, work, expected)
        runtime = median([str(binary)], 101, work, expected)

        # Confirm cross-target emitters are exercised independently of host architecture.
        for target, marker in (("x86_64", ".globl main"),
                               ("aarch64", "adrp"),
                               ("armv7", "ldr r0")):
            target_asm = work / f"{target}.s"
            subprocess.run([str(SWAN), str(source), "--target", target,
                            "--emit-asm", "-o", str(target_asm)], check=True)
            if marker not in target_asm.read_text():
                raise RuntimeError(f"{target} assembly marker missing")

        print("Swan C compiler benchmark (median wall-clock; lower is better)")
        print("Workload: one string log statement and integer return")
        print(f"C frontend + assembly emission: {emit:.2f} seconds")
        print(f"Assembler + native link:        {link:.2f} seconds")
        print(f"End-to-end compile + execute:   {compile_run:.2f} seconds")
        print(f"Prebuilt executable runtime:    {runtime:.2f} seconds")
        print("Cross-target assembly emission: x86_64 PASS, aarch64 PASS, armv7 PASS")
        print("Correctness: PASS")


if __name__ == "__main__":
    main()
