#!/usr/bin/env python3
import json, pathlib, subprocess, time

ROOT = pathlib.Path.cwd()
N = 100_000_000
OUT = ROOT / "benchmark-results"
OUT.mkdir(exist_ok=True)
goose_src = OUT / "hello.goose"
swan_src = OUT / "hello.swan"
goose_src.write_text('fn main() {\n' + f'    for i in {N} {{ print("Hello, World!"); }}\n' + '}\n')
swan_src.write_text('fn main() -> int:\n' + f'    for i in 0..{N}\n' + '        log "Hello, World!"\n    return 0\n')

def measured(command, label):
    memfile = OUT / (label + "-rss.txt")
    wrapped = ["/usr/bin/time", "-f", "%M", "-o", str(memfile), *map(str, command)]
    start = time.perf_counter()
    p = subprocess.run(wrapped, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    elapsed = time.perf_counter() - start
    rss = None
    if memfile.exists():
        values = [line.strip() for line in memfile.read_text().splitlines() if line.strip().isdigit()]
        rss = int(values[-1]) if values else None
    return p, elapsed, rss

goose = pathlib.Path("goose/build/goose").resolve()
swan = pathlib.Path("swan/build/swan").resolve()
exe = OUT / "hello-goose"
gbuild, gbuild_s, gbuild_kb = measured([goose, "build", "-o", exe, goose_src], "goose-build")
if gbuild.returncode:
    raise SystemExit("Goose build failed:\n" + gbuild.stderr.decode(errors="replace"))

run_mem = OUT / "goose-runtime-rss.txt"
cmd = ["/usr/bin/time", "-f", "%M", "-o", str(run_mem), str(exe)]
start = time.perf_counter()
p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
byte_count = 0
while True:
    chunk = p.stdout.read(1024 * 1024)
    if not chunk:
        break
    byte_count += len(chunk)
stderr = p.stderr.read()
rc = p.wait()
grun_s = time.perf_counter() - start
if rc:
    raise SystemExit("Goose workload failed:\n" + stderr.decode(errors="replace"))
grun_kb = int(run_mem.read_text().strip())
expected = N * len("Hello, World!\n".encode())
if byte_count != expected:
    raise SystemExit(f"Output mismatch: expected {expected} bytes, got {byte_count}")

sbuild, sbuild_s, sbuild_kb = measured([swan, swan_src, "--emit-asm", "-o", OUT / "hello.s"], "swan-compile")
swan_supported = sbuild.returncode == 0
swan_reason = "" if swan_supported else sbuild.stderr.decode(errors="replace").strip()
result = {
    "workload": "100,000,000 lines: Hello, World! newline",
    "goose": {"compile_seconds": round(gbuild_s, 4), "compile_peak_rss_kb": gbuild_kb,
              "run_seconds": round(grun_s, 4), "run_peak_rss_kb": grun_kb,
              "output_bytes": byte_count, "executable_bytes": exe.stat().st_size,
              "source_bytes": goose_src.stat().st_size, "status": "PASS"},
    "swan": {"compile_seconds": round(sbuild_s, 4), "compile_peak_rss_kb": sbuild_kb,
             "run_seconds": None, "run_peak_rss_kb": None, "output_bytes": None,
             "source_bytes": swan_src.stat().st_size,
             "status": "PASS" if swan_supported else "UNSUPPORTED", "reason": swan_reason},
    "method": "Ubuntu GitHub-hosted runner; wall-clock via perf_counter; peak RSS via /usr/bin/time in KiB; output streamed and counted without retaining it."
}
(OUT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
lines = [
    "# Swan vs Goose — 100M-line Hello World benchmark", "",
    f"Workload: {N:,} output lines (Hello, World! plus newline).",
    "Measurements are from this GitHub Actions runner; lower time and RSS are better.", "",
    "| Compiler | Compile time (seconds) | Compile peak memory (MiB) | Run time (seconds) | Runtime peak memory (MiB) | Output data | Status |",
    "|---|---:|---:|---:|---:|---:|---|",
    f"| Goose | {gbuild_s:.2f} | {gbuild_kb/1024:.2f} | {grun_s:.2f} | {grun_kb/1024:.2f} | {byte_count:,} bytes | PASS |",
    f"| Swan | {sbuild_s:.2f} | {sbuild_kb/1024:.2f} | N/A | N/A | N/A | {'Compile accepted; not executed' if swan_supported else 'UNSUPPORTED'} |",
    "", f"Expected output volume: **{expected:,} bytes** ({expected/1e9:.2f} GB decimal).",
    f"Goose executable: **{exe.stat().st_size:,} bytes**. Goose source: **{goose_src.stat().st_size:,} bytes**. Swan source: **{swan_src.stat().st_size:,} bytes**.",
    "", "## Interpretation", "",
    "Swan's current C compiler does not implement for-loop lowering, so it cannot execute this same 100-million-iteration program. Swan compile time and memory therefore measure rejection of the equivalent source, not successful compilation. No Swan runtime number is invented.",
    "", "Peak memory is process maximum resident set size. Output was streamed and counted by the harness, not saved to disk. Timings are specific to this runner.",
    "", "Raw measurements: results.json."
]
(OUT / "results.md").write_text("\n".join(lines) + "\n")
print("\n".join(lines))
