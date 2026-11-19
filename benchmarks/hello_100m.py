#!/usr/bin/env python3
import json, pathlib, subprocess, time

ROOT = pathlib.Path.cwd()
N = 100_000_000
OUT = ROOT / "benchmark-results"
OUT.mkdir(exist_ok=True)
goose_src = OUT / "hello.goose"
brew_src = OUT / "hello.brew"
goose_src.write_text('fn main() {\n' + f'    for i in {N} {{ print("Hello, World!"); }}\n' + '}\n')
brew_src.write_text('func main() -> int:\n' + f'    for i in 0..{N}\n' + '        !printin >> "Hello, World!"\n    return 0\n')

def measured(command, label):
    memfile = OUT / (label + "-rss.txt")
    memfile.unlink(missing_ok=True)
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
brew = pathlib.Path("brew/build/brew").resolve()
exe = OUT / "hello-goose"
brew_exe = OUT / "hello-brew"
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

sbuild, sbuild_s, sbuild_kb = measured([brew, "build", brew_src, "-o", brew_exe], "brew-compile")
brew_supported = sbuild.returncode == 0
brew_reason = "" if brew_supported else sbuild.stderr.decode(errors="replace").strip()
srun_s = srun_kb = brew_bytes = None
if brew_supported:
    brew_mem = OUT / "brew-runtime-rss.txt"
    start = time.perf_counter()
    sp = subprocess.Popen(["/usr/bin/time", "-f", "%M", "-o", str(brew_mem), str(brew_exe)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    brew_bytes = 0
    while True:
        chunk = sp.stdout.read(1024 * 1024)
        if not chunk: break
        brew_bytes += len(chunk)
    brew_stderr = sp.stderr.read()
    src = sp.wait()
    srun_s = time.perf_counter() - start
    if src: raise SystemExit("Brew workload failed: " + brew_stderr.decode(errors="replace"))
    values = [line.strip() for line in brew_mem.read_text().splitlines() if line.strip().isdigit()]
    srun_kb = int(values[-1]) if values else None
    if brew_bytes != expected: raise SystemExit(f"Brew output mismatch: expected {expected} bytes, got {brew_bytes}")
result = {
    "workload": "100,000,000 lines: Hello, World! newline",
    "goose": {"compile_seconds": round(gbuild_s, 4), "compile_peak_rss_kb": gbuild_kb,
              "run_seconds": round(grun_s, 4), "run_peak_rss_kb": grun_kb,
              "output_bytes": byte_count, "executable_bytes": exe.stat().st_size,
              "source_bytes": goose_src.stat().st_size,\n              "output_mib_per_second": round((byte_count / 1048576) / grun_s, 2), "status": "PASS"},
    "brew": {"compile_seconds": round(sbuild_s, 4), "compile_peak_rss_kb": sbuild_kb,
             "run_seconds": round(srun_s, 4) if srun_s is not None else None, "run_peak_rss_kb": srun_kb, "output_bytes": brew_bytes,
             "source_bytes": brew_src.stat().st_size,
             "status": "PASS" if brew_supported else "UNSUPPORTED", "reason": brew_reason},
    "method": "Ubuntu GitHub-hosted runner; wall-clock via perf_counter; peak RSS via /usr/bin/time in KiB; output streamed and counted without retaining it."
}
(OUT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
lines = [
    "# Brew vs Goose — 100M-line Hello World benchmark", "",
    f"Workload: {N:,} output lines (Hello, World! plus newline).",
    "Measurements are from this GitHub Actions runner; lower time and RSS are better.", "",
    "| Compiler | Compile time (seconds) | Compile peak memory (MiB) | Run time (seconds) | Runtime peak memory (MiB) | Output data | Status |",
    "|---|---:|---:|---:|---:|---:|---|",
    f"| Goose | {gbuild_s:.2f} | {gbuild_kb/1024:.2f} | {grun_s:.2f} | {grun_kb/1024:.2f} | {byte_count:,} bytes | PASS |",
    (f"| Brew | {sbuild_s:.2f} | {sbuild_kb/1024:.2f} | {srun_s:.2f} | {srun_kb/1024:.2f} | {brew_bytes:,} bytes | PASS |" if brew_supported else f"| Brew | {sbuild_s:.2f} | {sbuild_kb/1024:.2f} | N/A | N/A | N/A | UNSUPPORTED |"),
    "", f"Expected output volume: **{expected:,} bytes** ({expected/1e9:.2f} GB decimal).",
    "",
    "| Storage footprint | Goose | Brew |",
    "|---|---:|---:|",
    f"| Source file | {goose_src.stat().st_size:,} bytes | {brew_src.stat().st_size:,} bytes |",
    f"| Compiled executable | {exe.stat().st_size:,} bytes | {brew_exe.stat().st_size:,} bytes |" if brew_supported else f"| Compiled executable | {exe.stat().st_size:,} bytes | N/A |",
    f"| Output throughput | {byte_count / 1048576 / grun_s:.2f} MiB/s | {(brew_bytes / 1048576 / srun_s):.2f} MiB/s |" if brew_supported else f"| Output throughput | {byte_count / 1048576 / grun_s:.2f} MiB/s | N/A |",
    "", "## Interpretation", "",
    "Both compilers receive equivalent 100-million-iteration programs and must produce exactly the same output before runtime results are accepted. Compile time includes language compilation and native executable linking.",
    "", "Peak memory is process maximum resident set size. Source and executable sizes show disk footprint; output bytes and MiB/s show data emitted and throughput. Output was streamed and counted, not saved to disk. Timings are specific to this runner. Runtime results also reflect each language runtime and its standard-output buffering behavior.",
    *(["", "Brew compiler diagnostic: `" + brew_reason.replace("\\", "\\\\").replace("`", "\\`").replace("\n", " ") + "`"] if not brew_supported else []),
    "", "Raw measurements: results.json."
]
(OUT / "results.md").write_text("\n".join(lines) + "\n")
print("\n".join(lines))
