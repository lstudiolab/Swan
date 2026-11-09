#!/usr/bin/env python3
"""Attribute Swan compiler frontend time to each compiler header.

Uses Clang -ftime-trace Source events on main.c (the compiler's single
translation unit), and Swan's built-in --profile for real compile-stage costs.
All reported durations are seconds. Header timings are frontend parse/preprocess
attribution, not runtime function costs.
"""
from __future__ import annotations
import argparse, json, pathlib, platform, re, shutil, statistics, subprocess, tempfile, time
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = "Swan compiler pipeline profile (seconds)"
PROFILE = re.compile(r"^\\s{2}(.+?)\\s+([0-9]+(?:\\.[0-9]+)?) s")
WORKLOAD = ROOT / "benchmarks" / "compiler_pipeline.swan"

def stage_sample(compiler, source, output):
    started = time.perf_counter()
    proc = subprocess.run([str(compiler), "build", "--profile", str(source), "-o", str(output)],
                          cwd=ROOT, text=True, capture_output=True)
    wall = time.perf_counter() - started
    if proc.returncode:
        raise RuntimeError(f"Swan compile failed:\\n{proc.stderr[-10000:]}")
    if HEADER not in proc.stderr:
        raise RuntimeError("Swan --profile output was missing.")
    values = {}
    for line in proc.stderr.splitlines():
        match = PROFILE.match(line)
        if match:
            values[match.group(1).strip()] = float(match.group(2))
    if "Total wall time" not in values:
        raise RuntimeError("Swan profile omitted Total wall time.")
    values["Compiler process wall time"] = wall
    return values

def clang_sample(clang, trace_dir, sample):
    obj = trace_dir / f"main-{sample}.o"
    cmd = [clang, "-std=c11", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-Wpedantic",
           "-DSWAN_SIL_BOOTSTRAP", f"-I{ROOT / 'compiler' / 'include'}",
           "-ftime-trace", "-ftime-trace-granularity=0",
           "-c", str(ROOT / "main.c"), "-o", str(obj)]
    started = time.perf_counter()
    proc = subprocess.run(cmd, cwd=trace_dir, text=True, capture_output=True)
    wall = time.perf_counter() - started
    if proc.returncode:
        raise RuntimeError(f"Clang trace compile failed:\\n{proc.stderr[-10000:]}")
    trace = obj.with_suffix(".json")
    if not trace.is_file():
        raise RuntimeError(f"Clang did not produce {trace.name}")
    data = json.loads(trace.read_text(encoding="utf-8"))
    totals = defaultdict(float)
    for event in data.get("traceEvents", []):
        if event.get("name") != "Source" or not isinstance(event.get("dur"), (int, float)):
            continue
        detail = str(event.get("args", {}).get("detail", "")).replace("\\\\", "/")
        marker = "/compiler/"
        if marker in detail:
            name = "compiler/" + detail.split(marker, 1)[1].split(":", 1)[0]
            totals[name] += float(event["dur"]) / 1_000_000.0
    return dict(totals), wall

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=pathlib.Path, default=ROOT / "build" / "swan")
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--output-dir", type=pathlib.Path, default=ROOT / "benchmark-results")
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    clang = shutil.which("clang")
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}; run sh build.sh first.")
    if not clang:
        raise SystemExit("Clang is required for per-header -ftime-trace attribution.")
    if args.repeats < 1:
        parser.error("--repeats must be at least 1")

    with tempfile.TemporaryDirectory(prefix="swan-file-timing-") as temp:
        tmp = pathlib.Path(temp)
        trace_dir = tmp / "clang"
        trace_dir.mkdir()
        file_samples = defaultdict(list)
        clang_walls = []
        for i in range(args.repeats):
            values, wall = clang_sample(clang, trace_dir, i)
            clang_walls.append(wall)
            for name, seconds in values.items():
                file_samples[name].append(seconds)
        stage_samples = defaultdict(list)
        for i in range(args.repeats):
            for name, seconds in stage_sample(compiler, WORKLOAD, tmp / f"stage-{i}").items():
                stage_samples[name].append(seconds)

    rows = [{"file": name, "median_seconds": statistics.median(values),
             "min_seconds": min(values), "max_seconds": max(values)}
            for name, values in file_samples.items()]
    rows.sort(key=lambda row: row["median_seconds"], reverse=True)
    attributed_total = sum(row["median_seconds"] for row in rows)
    for row in rows:
        row["share_percent"] = row["median_seconds"] / attributed_total * 100 if attributed_total else 0
    stages = {name: statistics.median(values) for name, values in stage_samples.items()}
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    result = {
        "host": platform.platform(), "machine": platform.machine(), "unit": "seconds",
        "repeats": args.repeats,
        "compiler_file_method": "Clang -ftime-trace Source event durations attributed to compiler headers; median across independent compiler-TU builds.",
        "compiler_file_total_attributed_seconds": attributed_total,
        "compiler_file_rows": rows,
        "clang_translation_unit_wall_median_seconds": statistics.median(clang_walls),
        "swan_pipeline_workload": str(WORKLOAD.relative_to(ROOT)),
        "swan_pipeline_stage_medians_seconds": stages,
        "note": "Header timings are frontend source-processing costs, not runtime function costs or exclusive wall-clock phases. SIL pass sources are concatenated by build.sh and are not individually attributable in this header trace."
    }
    (output / "compiler-file-timing.json").write_text(json.dumps(result, indent=2) + "\\n")
    lines = ["# Swan compiler file timing benchmark", "",
             f"- Host: {result['host']} ({result['machine']})",
             f"- Repeats: {args.repeats}; statistic: median; unit: seconds",
             "- Per-file method: Clang frontend Source-event attribution for each included compiler header.",
             f"- Clang full translation-unit wall time: **{result['clang_translation_unit_wall_median_seconds']:.6f} s**",
             f"- Sum of attributed header source-processing time: **{attributed_total:.6f} s**", "",
             "## Time by compiler file", "",
             "| Rank | Compiler file | Median time (s) | Share of attributed time | Min (s) | Max (s) |",
             "|---:|---|---:|---:|---:|---:|"]
    for i, row in enumerate(rows, 1):
        lines.append(f"| {i} | {row['file']} | {row['median_seconds']:.6f} | {row['share_percent']:.2f}% | {row['min_seconds']:.6f} | {row['max_seconds']:.6f} |")
    lines += ["", "## Actual Swan compile pipeline stages", "",
              f"Workload: {WORKLOAD.relative_to(ROOT)}; median of {args.repeats} builds.", "",
              "| Stage | Median time (s) |", "|---|---:|"]
    for name, seconds in stages.items():
        lines.append(f"| {name} | {seconds:.9f} |")
    lines += ["", "## Interpretation", "",
              "Per-file values measure frontend processing attributed to each header during compilation of main.c. They are not exclusive runtime profiling and should not be summed as independent wall-clock phases. The stage table measures Swan's actual source-compilation pipeline.",
              "The SIL .swan passes are concatenated into one generated source file by build.sh; this benchmark reports them as part of the overall compiler build, not as individually timed source files.",
              "", "Raw medians and host metadata are in compiler-file-timing.json."]
    (output / "compiler-file-timing.md").write_text("\\n".join(lines) + "\\n")
    print("\\n".join(lines))

if __name__ == "__main__":
    main()
