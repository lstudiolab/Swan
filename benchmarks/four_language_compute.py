#!/usr/bin/env python3
"""Same one-billion-iteration integer workload in Swan, Goose, Rust and C."""
import json, os, pathlib, platform, re, shutil, statistics, subprocess, tempfile, time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
EXPECTED = 48000000020
COMPILE_RUNS = 3
RUNTIME_RUNS = 5
TIMER = "/usr/bin/time"

SRC = {
"Swan": """fn main() -> int:
    var total = 0
    var i = 0
    while i < 1000
        var j = 0
        while j < 1000
            var k = 0
            while k < 1000
                total = total + ((i * 31 + j * 17 + k * 13) % 97)
                k = k + 1
            j = j + 1
        i = i + 1
    log total
    return 0
""",
"Goose": """import std;
fn main() {
    var total: i64 = 0;
    var i: i64 = 0;
    while i < 1000 {
        var j: i64 = 0;
        while j < 1000 {
            var k: i64 = 0;
            while k < 1000 {
                total += (i * 31 + j * 17 + k * 13) % 97;
                k += 1;
            }
            j += 1;
        }
        i += 1;
    }
    print(total);
}
""",
"Rust": """fn main() {
    let mut total: i64 = 0;
    let mut i: i64 = 0;
    while i < 1000 {
        let mut j: i64 = 0;
        while j < 1000 {
            let mut k: i64 = 0;
            while k < 1000 {
                total += (i * 31 + j * 17 + k * 13) % 97;
                k += 1;
            }
            j += 1;
        }
        i += 1;
    }
    println!("{}", std::hint::black_box(total));
}
""",
"C": """#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    int64_t total = 0, i = 0;
    while (i < 1000) {
        int64_t j = 0;
        while (j < 1000) {
            int64_t k = 0;
            while (k < 1000) {
                total += (i * 31 + j * 17 + k * 13) % 97;
                ++k;
            }
            ++j;
        }
        ++i;
    }
    printf("%" PRId64 "\\n", total);
    return 0;
}
"""
}

def host():
    cpu = "unknown"
    try:
        for line in pathlib.Path("/proc/cpuinfo").read_text().splitlines():
            if line.lower().startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    except OSError: pass
    mem = {}
    try:
        for line in pathlib.Path("/proc/meminfo").read_text().splitlines():
            k, v = line.split(":", 1)
            if k in ("MemTotal", "MemAvailable"): mem[k] = v.strip()
    except (OSError, ValueError): pass
    return {"os": platform.platform(), "kernel": platform.release(),
            "architecture": platform.machine(), "cpu": cpu,
            "logical_cpus": os.cpu_count(), "memory": mem}

def run(cmd, cwd, expected=None):
    fmt = "SWAN_BENCH:%e,%U,%S,%M,%P,%F,%R,%w,%c"
    start = time.perf_counter()
    p = subprocess.run([TIMER, "-f", fmt, *map(str, cmd)], cwd=cwd,
                       text=True, capture_output=True)
    wall = time.perf_counter() - start
    m = re.search(r"SWAN_BENCH:([0-9.]+),([0-9.]+),([0-9.]+),([0-9]+),([^,]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+)", p.stderr)
    if p.returncode: raise RuntimeError(f"{cmd} failed ({p.returncode})\\n{p.stdout}\\n{p.stderr}")
    if not m: raise RuntimeError(f"GNU time metrics missing: {p.stderr}")
    elapsed, user, system, rss, cpu, major, minor, voluntary, involuntary = m.groups()
    output = p.stdout.strip()
    if expected is not None and output != expected:
        raise RuntimeError(f"checksum mismatch: expected {expected}, got {output!r}")
    return {"wall_s": wall, "user_cpu_s": float(user), "system_cpu_s": float(system),
            "cpu_s": float(user)+float(system), "peak_rss_mib": int(rss)/1024,
            "cpu_percent": cpu, "major_faults": int(major), "minor_faults": int(minor),
            "voluntary_switches": int(voluntary), "involuntary_switches": int(involuntary)}

def med(samples, key): return statistics.median(x[key] for x in samples)

def tool_version(command):
    p = subprocess.run(command, text=True, capture_output=True)
    text = p.stdout or p.stderr
    return text.splitlines()[0] if p.returncode == 0 and text else 'unavailable'

def main():
    if not SWAN.is_file(): raise SystemExit("Build Swan first: sh build.sh")
    goose = os.environ.get("GOOSE", "goose")
    cc, rustc = shutil.which("cc"), shutil.which("rustc")
    if not cc or not rustc: raise SystemExit("cc and rustc are required")
    host_info = host()
    versions = {
        'Swan': 'Swan compiler at commit ' + str(os.getenv('GITHUB_SHA', 'local')),
        'Goose': tool_version([goose, 'version']),
        'Rust': tool_version([rustc, '--version']),
        'C': tool_version([cc, '--version']),
    }
    exts = {"Swan":"swan", "Goose":"goose", "Rust":"rs", "C":"c"}
    with tempfile.TemporaryDirectory(prefix="swan-goose-rust-c-") as td:
        d = pathlib.Path(td)
        src = {}
        for name, body in SRC.items():
            src[name] = d / ("program." + exts[name])
            src[name].write_text(body, encoding="utf-8")
        cmds = {
          "Goose": lambda out:[goose,"build","-o",str(out),str(src["Goose"])],
          "Rust": lambda out:[rustc,"-C","opt-level=3","-C","target-cpu=native",str(src["Rust"]),"-o",str(out)],
          "C": lambda out:[cc,"-O3","-march=native","-DNDEBUG","-std=c11",str(src["C"]),"-o",str(out)]
        }
        flags = {
          "Swan":"Swan x86-64 assembly generation + cc assembler/linker",
          "Goose":"goose build defaults; native C backend",
          "Rust":"rustc -C opt-level=3 -C target-cpu=native",
          "C":"cc -O3 -march=native -DNDEBUG -std=c11"
        }
        # One untimed warm-up per toolchain.
        for name in ("Swan","Goose","Rust","C"):
            out = d / ("warm-" + name.lower())
            if name == "Swan":
                asm = d / "warm.s"
                subprocess.run([str(SWAN),str(src[name]),"--emit-asm","-o",str(asm)],cwd=ROOT,check=True,capture_output=True)
                subprocess.run([cc,"-x","assembler",str(asm),"-o",str(out)],cwd=ROOT,check=True,capture_output=True)
            else:
                subprocess.run(cmds[name](out),cwd=ROOT,check=True,capture_output=True)
            actual = subprocess.check_output([str(out)],text=True).strip()
            if actual != str(EXPECTED): raise RuntimeError(f"{name} warm-up checksum mismatch: {actual}")
        results, bins = {}, {}
        for name in ("Swan","Goose","Rust","C"):
            samples, final = [], None
            for i in range(COMPILE_RUNS):
                out = d / f"{name.lower()}-{i}"
                if name == "Swan":
                    asm = d / f"swan-{i}.s"
                    front = run([str(SWAN),str(src[name]),"--emit-asm","-o",str(asm)],ROOT)
                    link = run([cc,"-x","assembler",str(asm),"-o",str(out)],ROOT)
                    samples.append({"wall_s":front["wall_s"]+link["wall_s"],
                                    "cpu_s":front["cpu_s"]+link["cpu_s"],
                                    "peak_rss_mib":max(front["peak_rss_mib"],link["peak_rss_mib"]),
                                    "swan_codegen_s":front["wall_s"],"assembler_link_s":link["wall_s"]})
                else: samples.append(run(cmds[name](out),ROOT))
                final = out
            bins[name] = final
            results[name] = {"settings":flags[name],"source_bytes":src[name].stat().st_size,
                             "compile_samples":samples,"compile_median_s":med(samples,"wall_s"),
                             "compile_cpu_median_s":med(samples,"cpu_s"),
                             "compile_peak_rss_mib_median":med(samples,"peak_rss_mib"),
                             "binary_bytes":final.stat().st_size,"runtime_samples":[]}
        names = ["Swan","Goose","Rust","C"]
        for round_no in range(RUNTIME_RUNS):
            order = names[round_no:] + names[:round_no]
            for name in order:
                x = run([str(bins[name])],ROOT,str(EXPECTED))
                results[name]["runtime_samples"].append(x)
        for name in names:
            x = results[name]; samples = x["runtime_samples"]
            x["runtime_median_s"] = med(samples,"wall_s")
            x["runtime_min_s"] = min(s["wall_s"] for s in samples)
            x["runtime_max_s"] = max(s["wall_s"] for s in samples)
            x["runtime_cpu_median_s"] = med(samples,"cpu_s")
            x["runtime_user_cpu_median_s"] = med(samples,"user_cpu_s")
            x["runtime_system_cpu_median_s"] = med(samples,"system_cpu_s")
            x["runtime_peak_rss_mib_median"] = med(samples,"peak_rss_mib")
            x["runtime_cpu_percent_median"] = statistics.median(float(s["cpu_percent"].rstrip("%")) for s in samples)
            x["iterations_per_second"] = 1_000_000_000/x["runtime_median_s"]
        report = {"benchmark":"Swan vs Goose vs Rust vs C — one billion integer loop iterations",
                  "commit":os.getenv("GITHUB_SHA"),"host":host_info,"toolchain_versions":versions,
                  "workload":{"loop_dimensions":[1000,1000,1000],"iterations":1_000_000_000,
                    "operation":"total += ((i * 31 + j * 17 + k * 13) % 97)",
                    "signed_integer_width":64,"expected_checksum":EXPECTED,"correctness":"PASS",
                    "compile_repeats":COMPILE_RUNS,"runtime_repeats":RUNTIME_RUNS},
                  "languages":results,
                  "notes":["All programs execute the same loop and signed 64-bit arithmetic.",
                    "Swan compile time includes Swan assembly generation and host assembly/linking; sub-times are reported.",
                    "Goose uses its default build/backend optimization settings; Rust and C flags are explicit.",
                    "Runtime order rotates each round; every sample must produce the expected checksum.",
                    "Wall time uses perf_counter; GNU time provides CPU time, peak RSS, faults and context switches.",
                    "Peak RSS is per-process maximum resident memory, not total host RAM.",
                    "This common supported compute workload is distinct from the 20M binary-search workload; current Swan backend does not lower array allocation/indexing or break/continue."]}
        (ROOT/"four-language-benchmark.json").write_text(json.dumps(report,indent=2)+"\n")
        lines = ["# Swan vs Goose vs Rust vs C","",
          "Workload: 1,000,000,000 identical loop iterations. Checksum verified.",
          f"Host: {host_info['cpu']}; {host_info['logical_cpus']} logical CPUs; RAM {host_info['memory'].get('MemTotal','unknown')}.","",
          "| Language | Compile median (s) | Compile CPU (s) | Compile RAM (MiB) | Runtime median (s) | Runtime CPU (s) | Runtime RAM (MiB) | Binary (B) | Iterations/s |",
          "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
        for name in names:
            x=results[name]
            lines.append(f"| {name} | {x['compile_median_s']:.6f} | {x['compile_cpu_median_s']:.3f} | {x['compile_peak_rss_mib_median']:.2f} | {x['runtime_median_s']:.6f} | {x['runtime_cpu_median_s']:.3f} | {x['runtime_peak_rss_mib_median']:.2f} | {x['binary_bytes']} | {x['iterations_per_second']:.0f} |")
        lines += ["","## Runtime samples (seconds)","","| Round | Swan | Goose | Rust | C |","|---:|---:|---:|---:|---:|"]
        for i in range(RUNTIME_RUNS):
            lines.append("| "+str(i+1)+" | "+" | ".join(f"{results[n]['runtime_samples'][i]['wall_s']:.6f}" for n in names)+" |")
        lines += ["","## Toolchain versions",""]+[f"- **{n}:** {versions[n]}" for n in names]
        lines += ["","## Compiler settings",""]+[f"- **{n}:** {flags[n]}" for n in names]
        lines += ["","Expected checksum: 48,000,000,020. All samples must match or the run fails.",
                  "Raw per-sample metrics: four-language-benchmark.json.",
                  "Note: Swan's 20M-array binary-search support is not yet complete; this is the largest common supported compute workload."]
        md="\n".join(lines)+"\n"
        (ROOT/"four-language-benchmark.md").write_text(md,encoding="utf-8")
        print(md)

if __name__ == "__main__": main()
