#!/usr/bin/env python3
"""Heavy, equivalent compile-time benchmark: Goose, Brew, Rust, C++, C, and x86-64 ASM.

Builds one generated translation unit per language. It measures cold source-to-
native-executable time (compile + link), peak compiler-process RSS, source size,
and output size. Unique source/output paths are used for every sample to avoid
incremental-build cache hits. No generated program is timed as part of compile
time.

Prerequisites:
  - build/brew (or --brew / BREW_BIN)
  - Goose compiler (or --goose / GOOSE_BIN)
  - rustc, a C compiler, a C++ compiler, and GNU-compatible assembler driver

Run:
  sh build.sh
  python3 benchmarks/compile_speed.py --repeats 5 --functions 256 --operations 64

For Goose, set GOOSE_BIN=/path/to/goose. Use --allow-missing to report only
available toolchains; the default requires all six to prevent misleading
partial comparisons.
"""
from __future__ import annotations

import argparse
import os
import pathlib
import platform
import shutil
import statistics
import subprocess
import tempfile
import time
from dataclasses import dataclass

ROOT = pathlib.Path(__file__).resolve().parents[1]


@dataclass
class Toolchain:
    name: str
    executable: str
    command: list[str]
    version_args: tuple[str, ...] = ("--version",)


def generate_sources(directory: pathlib.Path, functions: int,
                     operations: int) -> dict[str, pathlib.Path]:
    paths: dict[str, pathlib.Path] = {}

    # Keep every generated helper reachable from main. Repeated dependent
    # arithmetic prevents the benchmark from being just declaration parsing.
    brew = []
    goose = []
    c = ["#include <stdint.h>", "#include <stddef.h>", ""]
    cpp = ["#include <cstdint>", "#include <cstddef>", ""]
    rust = ["#![allow(dead_code)]", ""]

    for i in range(functions):
        brew.extend([f"func workload_{i}(value: int) -> int:",
                     "    var x = value"])
        goose.extend([f"fn workload_{i}(value: i64) -> i64 {{",
                      "    var x: i64 = value;"])
        c.extend([f"static uint64_t workload_{i}(uint64_t value) {{",
                  "    uint64_t x = value;"])
        cpp.extend([f"static std::uint64_t workload_{i}(std::uint64_t value) {{",
                    "    std::uint64_t x = value;"])
        rust.extend([f"#[inline(never)] fn workload_{i}(value: u64) -> u64 {{",
                     "    let mut x = value;"])

        for j in range(operations):
            k = ((i + 1) * (j + 3) * 17) % 999_983 + 1
            m = (j * 13 + i * 7) % 97 + 3
            brew.append(f"    x = (x * {m} + {k}) % 1000003")
            goose.append(f"    x = (x * {m} + {k}) % 1000003;")
            c.append(f"    x = (x * UINT64_C({m}) + UINT64_C({k})) % UINT64_C(1000003);")
            cpp.append(f"    x = (x * UINT64_C({m}) + UINT64_C({k})) % UINT64_C(1000003);")
            rust.append(f"    x = (x * {m} + {k}) % 1000003;")

        brew.extend(["    return x", ""])
        goose.extend(["    return x;", "}", ""])
        c.extend(["    return x;", "}", ""])
        cpp.extend(["    return x;", "}", ""])
        rust.extend(["    x", "}", ""])

    brew.extend(["func main() -> int:", "    var total = 0"])
    goose.extend(["fn main() {", "    var total: i64 = 0;"])
    c.extend(["int main(void) {", "    uint64_t total = 0;"])
    cpp.extend(["int main() {", "    std::uint64_t total = 0;"])
    rust.extend(["fn main() {", "    let mut total: u64 = 0;"])

    for i in range(functions):
        arg = i % 997 + 1
        brew.append(f"    total = (total + workload_{i}({arg})) % 1000000007")
        goose.append(f"    total = (total + workload_{i}({arg})) % 1000000007;")
        c.append(f"    total = (total + workload_{i}(UINT64_C({arg}))) % UINT64_C(1000000007);")
        cpp.append(f"    total = (total + workload_{i}(UINT64_C({arg}))) % UINT64_C(1000000007);")
        rust.append(f"    total = (total + workload_{i}({arg})) % 1000000007;")

    brew.extend(["    return 0", ""])
    goose.extend(["    print(total);", "}", ""])
    c.extend(["    return (int)(total == UINT64_MAX);", "}", ""])
    cpp.extend(["    return static_cast<int>(total == UINT64_MAX);", "}", ""])
    rust.extend(["    std::hint::black_box(total);", "}", ""])

    sources = {
        "Brew": ("workload.brew", "\n".join(brew)),
        "Goose": ("workload.goose", "\n".join(goose)),
        "Rust": ("workload.rs", "\n".join(rust)),
        "C++": ("workload.cpp", "\n".join(cpp)),
        "C": ("workload.c", "\n".join(c)),
    }
    for language, (filename, text) in sources.items():
        path = directory / filename
        path.write_text(text + "\n", encoding="utf-8")
        paths[language] = path

    # A comparable instruction-volume assembly translation unit. This is
    # handwritten x86-64 GAS syntax, so its timing is assembler + linker only,
    # not a high-level language frontend/code-generation comparison.
    asm = [".text"]
    for i in range(functions):
        asm.extend([f".globl workload_{i}", f".type workload_{i}, @function",
                    f"workload_{i}:", "    movq %rdi, %rax"])
        for j in range(operations):
            k = ((i + 1) * (j + 3) * 17) % 999_983 + 1
            m = (j * 13 + i * 7) % 97 + 3
            asm.extend([f"    imulq ${m}, %rax", f"    addq ${k}, %rax",
                        "    andl $2147483647, %eax"])
        asm.extend(["    ret", f".size workload_{i}, .-workload_{i}", ""])
    asm.extend([".globl main", ".type main, @function", "main:",
                "    xorl %eax, %eax", "    ret", ".size main, .-main", ""])
    asm_path = directory / "workload.s"
    asm_path.write_text("\n".join(asm), encoding="utf-8")
    paths["ASM"] = asm_path
    return paths


def resolve_tool(value: str | None, fallback: str) -> str | None:
    candidate = value or fallback
    if pathlib.Path(candidate).is_file():
        return str(pathlib.Path(candidate).resolve())
    return shutil.which(candidate)


def version_of(executable: str, args: tuple[str, ...]) -> str:
    try:
        result = subprocess.run([executable, *args], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=10, check=False)
        return (result.stdout.strip().splitlines() or ["version unavailable"])[0]
    except (OSError, subprocess.TimeoutExpired):
        return "version unavailable"


def run_sample(command: list[str], cwd: pathlib.Path, output: pathlib.Path) -> tuple[float, int, int]:
    time_bin = shutil.which("time")
    if not time_bin:
        raise RuntimeError("GNU time is required for peak RSS measurement")
    # /usr/bin/time is needed because shell 'time' is not an executable.
    time_path = pathlib.Path("/usr/bin/time")
    if not time_path.exists():
        raise RuntimeError("GNU /usr/bin/time is required for peak RSS measurement")
    start = time.perf_counter()
    result = subprocess.run([str(time_path), "-f", "__RSS_KB__%M",
                              *command], cwd=cwd, text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    elapsed = time.perf_counter() - start
    rss = None
    diagnostics = []
    for line in result.stderr.splitlines():
        if line.startswith("__RSS_KB__"):
            rss = int(line.removeprefix("__RSS_KB__"))
        else:
            diagnostics.append(line)
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {' '.join(command)}\n"
            + "\n".join(diagnostics)[-10000:]
        )
    if not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError(f"Toolchain did not produce expected output: {output}")
    if rss is None:
        raise RuntimeError("GNU time did not report peak RSS")
    return elapsed, rss, output.stat().st_size


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--brew", default=os.environ.get("BREW_BIN", str(ROOT / "build" / "brew")))
    parser.add_argument("--goose", default=os.environ.get("GOOSE_BIN", "/tmp/goose-build/goose"))
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--rustc", default=os.environ.get("RUSTC", "rustc"))
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--functions", type=int, default=384)
    parser.add_argument("--operations", type=int, default=96)
    parser.add_argument("--allow-missing", action="store_true")
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "benchmark-results" / "compile-speed.md")
    args = parser.parse_args()
    if args.repeats < 1 or args.warmups < 0 or args.functions < 1 or args.operations < 1:
        parser.error("repeats/functions/operations must be positive; warmups cannot be negative")

    configured = {
        "Brew": resolve_tool(args.brew, args.brew),
        "Goose": resolve_tool(args.goose, args.goose),
        "Rust": resolve_tool(args.rustc, args.rustc),
        "C++": resolve_tool(args.cxx, args.cxx),
        "C": resolve_tool(args.cc, args.cc),
        "ASM": resolve_tool(args.cc, args.cc),
    }
    missing = [name for name, exe in configured.items() if not exe]
    if missing and not args.allow_missing:
        raise SystemExit("Missing toolchains: " + ", ".join(missing)
                         + ". Install them or pass --allow-missing.")

    available = {name: exe for name, exe in configured.items() if exe}
    if "Brew" not in available or "Goose" not in available:
        raise SystemExit("Brew and Goose are required; set --brew and --goose/GOOSE_BIN.")

    with tempfile.TemporaryDirectory(prefix="brew-compile-speed-") as temp:
        work = pathlib.Path(temp)
        generated = generate_sources(work, args.functions, args.operations)
        samples: dict[str, list[tuple[float, int, int]]] = {name: [] for name in available}

        def command_for(language: str, source: pathlib.Path, output: pathlib.Path) -> list[str]:
            if language == "Brew":
                return [available[language], "build", str(source), "-o", str(output)]
            if language == "Goose":
                return [available[language], "build", "-o", str(output), str(source)]
            if language == "Rust":
                return [available[language], "-C", "opt-level=2", "-C", "codegen-units=1",
                        str(source), "-o", str(output)]
            if language == "C++":
                return [available[language], "-std=c++20", "-O2", "-pipe", str(source), "-o", str(output)]
            if language == "C":
                return [available[language], "-std=c11", "-O2", "-pipe", str(source), "-o", str(output)]
            return [available[language], "-x", "assembler", "-no-pie", str(source), "-o", str(output)]

        # Warmups use separate paths and are excluded from all statistics.
        for language in available:
            for warmup in range(args.warmups):
                source = generated[language]
                output = work / f"warm-{language}-{warmup}"
                run_sample(command_for(language, source, output), work, output)

        # Rotate language order each repetition to reduce systematic thermal/
        # frequency bias from always measuring the same compiler first.
        languages = list(available)
        for repeat in range(args.repeats):
            order = languages[repeat % len(languages):] + languages[:repeat % len(languages)]
            for language in order:
                source = generated[language]
                # A unique copy path prevents toolchains from reusing build caches.
                unique_source = work / f"{language}-{repeat}{source.suffix}"
                unique_source.write_bytes(source.read_bytes())
                output = work / f"{language}-{repeat}.bin"
                samples[language].append(run_sample(
                    command_for(language, unique_source, output), work, output
                ))

        rows = {}
        for language, measurements in samples.items():
            rows[language] = {
                "median_seconds": statistics.median(x[0] for x in measurements),
                "min_seconds": min(x[0] for x in measurements),
                "median_peak_rss_kib": statistics.median(x[1] for x in measurements),
                "median_binary_bytes": statistics.median(x[2] for x in measurements),
                "source_bytes": generated[language].stat().st_size,
                "version": version_of(available[language],
                                      ("--version",) if language != "Goose" else ("--version",)),
            }

    flags = {
        "Brew": "native build defaults",
        "Goose": "Goose native defaults",
        "Rust": "-C opt-level=2 -C codegen-units=1",
        "C++": "-std=c++20 -O2 -pipe",
        "C": "-std=c11 -O2 -pipe",
        "ASM": "GAS x86-64; -x assembler -no-pie",
    }
    for language in rows:
        rows[language]["flags"] = flags[language]

    print("Heavy native compilation benchmark")
    print(f"Host: {platform.platform()} ({platform.machine()})")
    print(f"Workload: {args.functions} functions x {args.operations} dependent operations "
          f"= {args.functions * args.operations:,} arithmetic statements per high-level language")
    print(f"Samples: {args.repeats} measured + {args.warmups} warm-up per toolchain")
    print("Metric: cold source-to-executable wall time (compile + link), median; lower is faster")
    print("ASM caveat: handwritten x86-64 GAS is assembled and linked; it has no high-level frontend.")
    print()
    print(f"{'Language':<10} {'Median s':>12} {'Best s':>12} {'Peak RSS MiB':>14} {'Source KiB':>12} {'Binary KiB':>12}")
    print("-" * 76)
    for language in ("Goose", "Brew", "Rust", "C++", "C", "ASM"):
        if language not in rows:
            continue
        row = rows[language]
        print(f"{language:<10} {row['median_seconds']:>12.4f} {row['min_seconds']:>12.4f} "
              f"{row['median_peak_rss_kib'] / 1024:>14.1f} "
              f"{row['source_bytes'] / 1024:>12.1f} "
              f"{row['median_binary_bytes'] / 1024:>12.1f}")

    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Heavy native compilation benchmark", "",
        f"- Host: {platform.platform()} ({platform.machine()})",
        f"- Workload: {args.functions} functions × {args.operations} dependent arithmetic statements",
        f"- Samples: {args.repeats} measured, {args.warmups} warm-up per toolchain",
        "- Measurement: cold source-to-native-executable wall time, including linking",
        "- Statistic: median; best sample shown for context",
        "- Peak memory: process maximum resident set size (RSS)",
        "- Compiler caches: unique source and output paths for every measured run",
        "- Correctness: compilation success and non-empty executable checked; runtime is intentionally excluded",
        "- ASM: handwritten x86-64 GAS; assembly + link only, not comparable to a high-level frontend",
        "", "| Language | Median compile + link (s) | Best (s) | Median peak RSS (MiB) | Source (KiB) | Binary (KiB) | Toolchain | Flags |",
        "|---|---:|---:|---:|---:|---:|---|---|",
    ]
    for language in ("Goose", "Brew", "Rust", "C++", "C", "ASM"):
        if language not in rows:
            continue
        row = rows[language]
        lines.append(
            f"| {language} | {row['median_seconds']:.4f} | {row['min_seconds']:.4f} | "
            f"{row['median_peak_rss_kib'] / 1024:.1f} | {row['source_bytes'] / 1024:.1f} | "
            f"{row['median_binary_bytes'] / 1024:.1f} | {row['version'].replace('|', '/')} | "
            f"{row['flags']} |"
        )
    lines += ["", "## Interpretation notes", "",
              "This is a compile-time benchmark, not a runtime benchmark. The same generated arithmetic workload shape is used across Goose, Brew, Rust, C++, and C. Compiler versions, optimization flags, source sizes, and peak RSS are recorded so results can be interpreted rather than treated as a universal ranking.",
              "",
              "The assembly row measures the assembler and linker on handwritten machine-level source. It is a useful lower-level toolchain reference, but it does not measure a language frontend or compiler optimization pipeline.",
              "",
              "Run on an otherwise idle machine, use the same machine and toolchain versions, and compare medians across repeated runs. Do not compare results from different hardware or build flags."]
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"\nDetailed report: {output}")


if __name__ == "__main__":
    main()
