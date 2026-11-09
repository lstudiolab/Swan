#!/usr/bin/env python3
"""Swan vs Goose: print one billion deterministic pseudo-random integers."""
import json, os, pathlib, platform, statistics, subprocess, tempfile, time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
GOOSE = pathlib.Path(os.environ.get("GOOSE", "goose"))
COUNT = int(os.environ.get("RANDOM_PRINT_COUNT", "1000000000"))
SEED, MULTIPLIER, MODULUS, CHECK_MOD = 1, 48271, 2147483647, 1000000007
COMPILE_REPEATS, RUNTIME_REPEATS = 3, 3


def source(language, emit):
    if language == "Swan":
        output = "        log state\n" if emit else ""
        return f"""fn main() -> int:
    var i = 0
    var state = {SEED}
    var checksum = 0
    while i < {COUNT}
        state = (state * {MULTIPLIER}) % {MODULUS}
{output}        checksum = (checksum + state) % {CHECK_MOD}
        i = i + 1
    log checksum
    return 0
"""
    output = "        print(state);\n" if emit else ""
    return f"""fn main() {{
    var i: i64 = 0;
    var state: i64 = {SEED};
    var checksum: i64 = 0;
    while i < {COUNT} {{
        state = (state * {MULTIPLIER}) % {MODULUS};
{output}        checksum = (checksum + state) % {CHECK_MOD};
        i += 1;
    }}
    print(checksum);
}}
"""


def command(language, src, binary):
    if language == "Swan":
        return [str(SWAN), "build", str(src), "-o", str(binary)]
    return [str(GOOSE), "build", "-o", str(binary), str(src)]


def compile_one(language, src, binary):
    cmd = command(language, src, binary)
    start = time.perf_counter()
    result = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(f"{language} compilation failed: {' '.join(cmd)}\n{result.stderr}")
    return elapsed


def checksum(binary, language):
    result = subprocess.run([str(binary)], cwd=ROOT, text=True, capture_output=True, check=True)
    try:
        return int(result.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError) as error:
        raise RuntimeError(f"{language} emitted invalid checksum: {result.stdout[-200:]}") from error


def runtime(binary):
    start = time.perf_counter()
    with open(os.devnull, "wb") as sink:
        result = subprocess.run([str(binary)], cwd=ROOT, stdout=sink, stderr=subprocess.PIPE)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace"))
    return elapsed


def main():
    if COUNT < 1 or not SWAN.is_file():
        raise SystemExit("Set a positive RANDOM_PRINT_COUNT and build Swan with sh build.sh")
    if not GOOSE.is_file() and not os.environ.get("GOOSE"):
        raise SystemExit("Goose not found; set GOOSE to the Goose executable")
    languages = ("Swan", "Goose")
    compiles = {name: [] for name in languages}
    runs = {name: [] for name in languages}

    with tempfile.TemporaryDirectory(prefix="swan-goose-random-print-") as td:
        work = pathlib.Path(td)
        for sample in range(COMPILE_REPEATS):
            for name in languages:
                ext = "swan" if name == "Swan" else "goose"
                src, binary = work / f"{name}-{sample}.{ext}", work / f"{name}-{sample}"
                src.write_text(source(name, True), encoding="utf-8")
                compiles[name].append(compile_one(name, src, binary))

        checksums, binaries = {}, {}
        for name in languages:
            ext = "swan" if name == "Swan" else "goose"
            verify_src, verify_bin = work / f"verify.{ext}", work / f"verify-{name}"
            verify_src.write_text(source(name, False), encoding="utf-8")
            compile_one(name, verify_src, verify_bin)
            checksums[name] = checksum(verify_bin, name)
            run_src, run_bin = work / f"runtime.{ext}", work / f"runtime-{name}"
            run_src.write_text(source(name, True), encoding="utf-8")
            compile_one(name, run_src, run_bin)
            binaries[name] = run_bin

        if checksums["Swan"] != checksums["Goose"]:
            raise RuntimeError(f"Checksum mismatch: Swan={checksums['Swan']} Goose={checksums['Goose']}")

        for name in languages:
            runtime(binaries[name])
        for sample in range(RUNTIME_REPEATS):
            order = languages if sample % 2 == 0 else tuple(reversed(languages))
            for name in order:
                runs[name].append(runtime(binaries[name]))

    compile_median = {n: statistics.median(compiles[n]) for n in languages}
    run_median = {n: statistics.median(runs[n]) for n in languages}
    faster_compile = min(languages, key=lambda n: compile_median[n])
    faster_runtime = min(languages, key=lambda n: run_median[n])
    result = {
        "workload": "print one billion deterministic pseudo-random integers",
        "iterations": COUNT,
        "generator": f"Park-Miller LCG: state = state * {MULTIPLIER} mod {MODULUS}",
        "seed": SEED,
        "checksum": checksums["Swan"],
        "host": {"platform": platform.platform(), "machine": platform.machine(),
                 "processor": platform.processor() or "not reported"},
        "samples": {"compile": compiles, "runtime": runs},
        "median_seconds": {"compile": compile_median, "runtime": run_median},
        "winner": {"compile": faster_compile, "runtime": faster_runtime},
        "correctness": "PASS: both verification executables produced the same checksum",
        "method": "Runtime stdout redirected to /dev/null; all print calls execute. Compile and runtime medians use three samples; runtime has one warm-up per language.",
        "caveat": "Deterministic pseudo-random sequence, not cryptographic randomness. Runtime includes integer formatting and line printing, excludes terminal and disk throughput."
    }
    out = ROOT / "benchmark-results"
    out.mkdir(exist_ok=True)
    (out / "random-print-1b.json").write_text(json.dumps(result, indent=2) + "\n")
    lines = [
        "# Swan vs Goose — 1 billion random-number prints", "",
        f"- Workload: **{COUNT:,} pseudo-random integers**, one decimal integer per line",
        f"- Generator: Park–Miller LCG (seed {SEED}, multiplier {MULTIPLIER}, modulus {MODULUS:,})",
        f"- Host: {platform.platform()} / {platform.machine()}",
        "- Samples: 3 compile medians, 3 runtime medians, one runtime warm-up per language",
        "- Runtime output redirected to /dev/null; all print calls execute, terminal and disk speed excluded.",
        f"- Correctness: **PASS** — matching checksum {checksums['Swan']}", "",
        "| Language | Compile time (s, median) | Runtime (s, median) |",
        "|---|---:|---:|"
    ]
    for name in languages:
        lines.append(f"| {name} | {compile_median[name]:.6f} | {run_median[name]:.6f} |")
    lines += ["", f"- Faster compilation: **{faster_compile}**",
              f"- Faster runtime: **{faster_runtime}**", "",
              "The verification twin runs the same PRNG and checksum loop without printing, so both sequences can be compared without piping more than 10 GB through a timed consumer.",
              "Runtime includes PRNG arithmetic, integer formatting, and one line-print call per iteration.",
              "Raw samples and host details: random-print-1b.json."]
    report = "\n".join(lines) + "\n"
    (out / "random-print-1b.md").write_text(report)
    print(report, flush=True)


if __name__ == "__main__":
    main()
