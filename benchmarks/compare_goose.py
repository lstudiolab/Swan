#!/usr/bin/env python3
"""Same-source-workload Brew vs Goose benchmark for Linux CI."""
from __future__ import annotations

import os
import pathlib
import statistics
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
BREW = ROOT / "build" / "brew"
GOOSE = pathlib.Path(os.environ.get("GOOSE_BIN", "/tmp/goose-build/goose"))
REPEATS = 5
RUNTIME_REPEATS = 7
I_LIMIT = 1_000
J_LIMIT = 1_000_000


def expected_checksum() -> int:
    # Count j residues modulo 97, then sum each i row independently.
    counts = [J_LIMIT // 97 + (r < J_LIMIT % 97) for r in range(97)]
    total = 0
    for i in range(I_LIMIT):
        total += sum(counts[r] * ((31 * i + 17 * r) % 97) for r in range(97))
    return total


def timed(command: list[str], env: dict[str, str]) -> float:
    start = time.perf_counter()
    subprocess.run(command, env=env, stdout=subprocess.DEVNULL,
                   stderr=subprocess.PIPE, check=True, text=True)
    return time.perf_counter() - start


def median_build(command_factory, env: dict[str, str], work: pathlib.Path,
                 language: str, mode: str) -> float:
    samples = []
    for index in range(REPEATS):
        source = work / f"{language}_{mode}_{index}{'.brew' if language == 'brew' else '.goose'}"
        source.write_text(BREW_SOURCE if language == "brew" else GOOSE_SOURCE,
                          encoding="utf-8")
        output = work / f"{language}_{mode}_{index}{'.s' if language == 'brew' else '.c'}"
        samples.append(timed(command_factory(source, output), env))
    return statistics.median(samples)


BREW_SOURCE = """func main() -> int:
    var total = 0
    var i = 0
    while i < 1000
        var j = 0
        while j < 1000000
            total = total + ((i * 31 + j * 17) % 97)
            j = j + 1
        i = i + 1
    !printin >> total
    return 0
"""

GOOSE_SOURCE = """fn main() {
    var total: i64 = 0;
    var i: i64 = 0;
    while i < 1000 {
        var j: i64 = 0;
        while j < 1000000 {
            total += (i * 31 + j * 17) % 97;
            j += 1;
        }
        i += 1;
    }
    print(total);
}
"""


def main() -> None:
    if not BREW.is_file() or not GOOSE.is_file():
        raise SystemExit("Build Brew and Goose compilers before running this benchmark.")

    env = os.environ.copy()
    env.update({"CC": "cc", "GOOSE_CC": "cc"})
    expected = str(expected_checksum())

    with tempfile.TemporaryDirectory(prefix="brew-goose-bench-") as directory:
        work = pathlib.Path(directory)

        def brew_emit(source: pathlib.Path, output: pathlib.Path) -> list[str]:
            return [str(BREW), str(source), "--emit-asm", "-o", str(output)]

        def goose_emit(source: pathlib.Path, output: pathlib.Path) -> list[str]:
            return [str(GOOSE), "-o", str(output), str(source)]

        def brew_build(source: pathlib.Path, output: pathlib.Path) -> list[str]:
            return [str(BREW), "build", str(source), "-o", str(output)]

        def goose_build(source: pathlib.Path, output: pathlib.Path) -> list[str]:
            return [str(GOOSE), "build", str(source), "-o", str(output)]

        # Warm each compiler and Goose's one-time native runtime build outside
        # measured samples. Each measured source has a unique filename.
        for language, content, command in (
            ("brew", BREW_SOURCE, brew_build),
            ("goose", GOOSE_SOURCE, goose_build),
        ):
            source = work / f"warmup.{language}"
            binary = work / f"warmup-{language}"
            source.write_text(content, encoding="utf-8")
            subprocess.run(command(source, binary), env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            actual = subprocess.check_output([str(binary)], env=env, text=True).strip()
            if actual != expected:
                raise RuntimeError(f"{language} warm-up checksum mismatch: {actual} != {expected}")

        brew_frontend = median_build(brew_emit, env, work, "brew", "frontend")
        goose_frontend = median_build(goose_emit, env, work, "goose", "frontend")
        brew_build_time = median_build(brew_build, env, work, "brew", "native")
        goose_build_time = median_build(goose_build, env, work, "goose", "native")

        # Rebuild one final executable for runtime timing; builds are not timed.
        runtime_binaries = {}
        for language, content, command in (
            ("brew", BREW_SOURCE, brew_build),
            ("goose", GOOSE_SOURCE, goose_build),
        ):
            source = work / f"runtime.{language}"
            binary = work / f"runtime-{language}"
            source.write_text(content, encoding="utf-8")
            subprocess.run(command(source, binary), env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            runtime_binaries[language] = binary

        runtime_times = {}
        for language, binary in runtime_binaries.items():
            subprocess.run([str(binary)], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            samples = []
            for _ in range(RUNTIME_REPEATS):
                start = time.perf_counter()
                result = subprocess.run([str(binary)], env=env, capture_output=True,
                                        text=True, check=True)
                samples.append(time.perf_counter() - start)
                if result.stdout.strip() != expected:
                    raise RuntimeError(
                        f"{language} checksum mismatch: {result.stdout.strip()} != {expected}"
                    )
            runtime_times[language] = statistics.median(samples)

        print("Brew vs Goose — identical native benchmark")
        print(f"Workload: {I_LIMIT:,} x {J_LIMIT:,} = {I_LIMIT * J_LIMIT:,} loop iterations")
        print("Brew: direct x86-64 assembly; Goose: native build with its default backend")
        print(f"Verified checksum: {expected}")
        print(f"Samples: {REPEATS} compile samples, {RUNTIME_REPEATS} runtime samples (median)")
        print()
        print(f"{'Measurement':<30}{'Brew':>14}{'Goose':>14}")
        print("-" * 58)
        print(f"{'Frontend + native-code emission':<30}{brew_frontend:>13.2f}s{goose_frontend:>13.2f}s")
        print(f"{'Source to native executable':<30}{brew_build_time:>13.2f}s{goose_build_time:>13.2f}s")
        print(f"{'Prebuilt executable runtime':<30}{runtime_times['brew']:>13.2f}s{runtime_times['goose']:>13.2f}s")
        print()
        print("Correctness: PASS (both executables produced the same checksum)")


if __name__ == "__main__":
    main()
