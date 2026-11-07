#!/usr/bin/env python3
"""Ten-way native/JVM runtime comparison on a shared branch-and-call workload."""
import json, os, pathlib, platform, re, statistics, subprocess, time

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "ten-language-benchmark"
OUT.mkdir(exist_ok=True)
N = 100_000_000
EXPECTED = 4_799_999_968
RUNS = 5
COMPILE_RUNS = 3
TIME = "/usr/bin/time"

SRC = {
"Swan": r"""fn transform(x: int) -> int:
    return (x * 31 + 7) % 97

fn main() -> int:
    var total = 0
    var i = 0
    while i < 100000000
        if i % 2 == 0
            total = total + transform(i)
        else
            total = total + transform(i + 1)
        i = i + 1
    log total
    return 0
""",
"Goose": r"""fn transform(x: i64) -> i64 {
    return (x * 31 + 7) % 97;
}
fn main() {
    var total: i64 = 0;
    for i in 0..100000000 {
        if i % 2 == 0 { total += transform(i); }
        else { total += transform(i + 1); }
    }
    print(total);
}
""",
"C": r"""#include <inttypes.h>
#include <stdio.h>
__attribute__((noinline)) static int64_t transform(int64_t x) { return (x * 31 + 7) % 97; }
int main(void) {
    int64_t total = 0;
    for (int64_t i = 0; i < 100000000; ++i) {
        if ((i % 2) == 0) total += transform(i);
        else total += transform(i + 1);
    }
    printf("%" PRId64 "\n", total);
    return 0;
}
""",
"C++": r"""#include <cstdint>
#include <cstdio>
__attribute__((noinline)) static int64_t transform(int64_t x) { return (x * 31 + 7) % 97; }
int main() {
    int64_t total = 0;
    for (int64_t i = 0; i < 100000000; ++i) {
        if ((i % 2) == 0) total += transform(i);
        else total += transform(i + 1);
    }
    std::printf("%lld\n", (long long)total);
}
""",
"Objective-C": r"""#include <stdio.h>
__attribute__((objc_root_class))
@interface SwanBench
+ (long long)transform:(long long)x;
@end
@implementation SwanBench
+ (long long)transform:(long long)x { return (x * 31 + 7) % 97; }
@end
int main(void) {
    long long total = 0;
    for (long long i = 0; i < 100000000LL; ++i) {
        if ((i % 2) == 0) total += [SwanBench transform:i];
        else total += [SwanBench transform:(i + 1)];
    }
    printf("%lld\n", total);
    return 0;
}
""",
"Go": r"""package main
import "fmt"
//go:noinline
func transform(x int64) int64 { return (x*31 + 7) % 97 }
func main() {
    var total int64
    for i := int64(0); i < 100000000; i++ {
        if i%2 == 0 { total += transform(i) } else { total += transform(i+1) }
    }
    fmt.Println(total)
}
""",
"Zig": r"""const std = @import("std");
noinline fn transform(x: i64) i64 { return @rem(x * 31 + 7, 97); }
pub fn main() void {
    var total: i64 = 0;
    var i: i64 = 0;
    while (i < 100000000) : (i += 1) {
        if (@rem(i, 2) == 0) total += transform(i)
        else total += transform(i + 1);
    }
    std.debug.print("{d}\n", .{total});
}
""",
"Kotlin": r"""interface Transformer { fun apply(x: Long): Long }
class ModuloTransformer : Transformer {
    override fun apply(x: Long): Long = (x * 31L + 7L) % 97L
}
fun main() {
    val transform: Transformer = ModuloTransformer()
    var total = 0L
    var i = 0L
    while (i < 100000000L) {
        if (i % 2L == 0L) total += transform.apply(i)
        else total += transform.apply(i + 1L)
        i++
    }
    println(total)
}
""",
"Rust": r"""#[inline(never)]
fn transform(x: i64) -> i64 { (x * 31 + 7) % 97 }
fn main() {
    let mut total: i64 = 0;
    for i in 0_i64..100_000_000_i64 {
        if i % 2 == 0 { total += transform(i); }
        else { total += transform(i + 1); }
    }
    println!("{}", std::hint::black_box(total));
}
""",
"ASM": r""".intel_syntax noprefix
.section .rodata
fmt: .string "%ld\n"
.text
.type transform, @function
transform:
    mov eax, edi
    imul eax, eax, 31
    add eax, 7
    xor edx, edx
    mov ecx, 97
    div ecx
    mov eax, edx
    ret
.size transform, .-transform
.globl main
.type main, @function
main:
    push r12
    push r13
    xor r12d, r12d
    xor r13d, r13d
.Lloop:
    cmp r12, 100000000
    jge .Ldone
    test r12b, 1
    jnz .Lodd
    mov rdi, r12
    call transform
    jmp .Lacc
.Lodd:
    lea rdi, [r12 + 1]
    call transform
.Lacc:
    add r13, rax
    inc r12
    jmp .Lloop
.Ldone:
    lea rdi, [rip + fmt]
    mov rsi, r13
    xor eax, eax
    call printf@PLT
    xor eax, eax
    pop r13
    pop r12
    ret
.size main, .-main
.section .note.GNU-stack,"",@progbits
"""
}

EXT = {"Swan":"swan","Goose":"goose","C":"c","C++":"cpp","Objective-C":"m",
       "Go":"go","Zig":"zig","Kotlin":"kt","Rust":"rs","ASM":"s"}
ORDER = ["Swan","Goose","C","C++","Objective-C","Go","Zig","Kotlin","Rust","ASM"]

def timed(command, cwd=ROOT):
    metrics = OUT / "time.txt"
    metrics.unlink(missing_ok=True)
    start = time.perf_counter()
    p = subprocess.run([TIME, "-f", "SWAN_BENCH:%e,%U,%S,%M", "-o", str(metrics),
                        *map(str, command)], cwd=cwd, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    wall = time.perf_counter() - start
    if p.returncode:
        raise RuntimeError(f"Command failed ({p.returncode}): {' '.join(map(str,command))}\n{p.stdout}\n{p.stderr}")
    m = re.search(r"SWAN_BENCH:([0-9.]+),([0-9.]+),([0-9.]+),([0-9]+)", metrics.read_text())
    if not m: raise RuntimeError(f"GNU time metrics missing for {command}")
    elapsed, user, system, rss = m.groups()
    return {"wall_s": wall, "gnu_wall_s": float(elapsed), "cpu_s": float(user)+float(system),
            "user_s": float(user), "system_s": float(system), "peak_rss_mib": int(rss)/1024,
            "stdout": p.stdout, "stderr": p.stderr}

def output_checksum(sample, name):
    text = sample["stdout"] + "\n" + sample["stderr"]
    values = re.findall(r"(?m)^\s*(-?\d+)\s*$", text)
    if not values or int(values[-1]) != EXPECTED:
        raise RuntimeError(f"{name} checksum mismatch; expected {EXPECTED}, output was:\n{text[-2000:]}")
    return int(values[-1])

def median(samples, key):
    return statistics.median(x[key] for x in samples)

def version(cmd):
    p = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return (p.stdout or p.stderr).splitlines()[0] if p.returncode == 0 else "unavailable"

def main():
    srcdir, bindir = OUT/"src", OUT/"bin"
    srcdir.mkdir(exist_ok=True); bindir.mkdir(exist_ok=True)
    sources = {}
    for name in ORDER:
        sources[name] = srcdir / ("program." + EXT[name])
        sources[name].write_text(SRC[name], encoding="utf-8")

    swan = ROOT/"build/swan"
    goose = ROOT/"goose/build/goose"
    zig = pathlib.Path(os.environ["ZIG"])
    kotlin = pathlib.Path(os.environ["KOTLINC"])
    compilers = {
      "Swan": lambda s,o: [swan, s, "--emit-asm", "-o", o],
      "Goose": lambda s,o: [goose, "build", "-o", o, s],
      "C": lambda s,o: ["cc","-O3","-march=native","-DNDEBUG","-std=c11",s,"-o",o],
      "C++": lambda s,o: ["c++","-O3","-march=native","-DNDEBUG","-std=c++17",s,"-o",o],
      "Objective-C": lambda s,o: ["gcc","-O3","-march=native","-DNDEBUG","-x","objective-c",s,"-lobjc","-o",o],
      "Go": lambda s,o: ["go","build","-trimpath","-ldflags=-s -w","-o",o,s],
      "Zig": lambda s,o: [zig,"build-exe","-O","ReleaseFast","-mcpu=native",s,"-femit-bin="+str(o)],
      "Kotlin": lambda s,o: [kotlin,s,"-include-runtime","-d",o],
      "Rust": lambda s,o: ["rustc","-C","opt-level=3","-C","target-cpu=native",s,"-o",o],
      "ASM": lambda s,o: ["cc","-no-pie",s,"-o",o],
    }
    versions = {
      "Swan": "repository compiler",
      "Goose": version([str(goose),"version"]),
      "C": version(["cc","--version"]),
      "C++": version(["c++","--version"]),
      "Objective-C": version(["gcc","--version"]),
      "Go": version(["go","version"]),
      "Zig": version([str(zig),"version"]),
      "Kotlin": version([str(kotlin),"-version"]),
      "Rust": version(["rustc","--version"]),
      "ASM": version(["as","--version"]),
    }
    bins = {}
    results = {}
    for name in ORDER:
        compile_samples = []
        for attempt in range(COMPILE_RUNS):
            out = bindir / (name.lower().replace("+","p").replace("-","") + (".jar" if name=="Kotlin" else ""))
            if name == "Swan":
                asm = bindir / f"swan-{attempt}.s"
                front = timed(compilers[name](sources[name],asm))
                link = timed(["cc","-no-pie",asm,"-o",out])
                compile_samples.append({"wall_s":front["wall_s"]+link["wall_s"],
                    "cpu_s":front["cpu_s"]+link["cpu_s"],
                    "peak_rss_mib":max(front["peak_rss_mib"],link["peak_rss_mib"]),
                    "codegen_s":front["wall_s"],"link_s":link["wall_s"]})
            else:
                sample = timed(compilers[name](sources[name],out))
                compile_samples.append({k:sample[k] for k in ("wall_s","cpu_s","peak_rss_mib")})
            if not out.exists(): raise RuntimeError(f"{name} did not produce {out}")
            bins[name] = out
        results[name] = {
          "toolchain": versions[name],
          "compile_samples": compile_samples,
          "compile_median_s": median(compile_samples,"wall_s"),
          "compile_cpu_median_s": median(compile_samples,"cpu_s"),
          "compile_peak_rss_mib_median": median(compile_samples,"peak_rss_mib"),
          "artifact_bytes": bins[name].stat().st_size,
          "source_bytes": sources[name].stat().st_size,
          "runtime_samples": []
        }

    for name in ORDER:
        cmd = ["java","-jar",bins[name]] if name=="Kotlin" else [bins[name]]
        output_checksum(timed(cmd),name+" warm-up")
    for round_no in range(RUNS):
        order = ORDER[round_no:] + ORDER[:round_no]
        for name in order:
            cmd = ["java","-jar",bins[name]] if name=="Kotlin" else [bins[name]]
            sample = timed(cmd)
            output_checksum(sample,name)
            results[name]["runtime_samples"].append({k:sample[k] for k in
                ("wall_s","gnu_wall_s","cpu_s","user_s","system_s","peak_rss_mib")})

    for name in ORDER:
        r=results[name]; samples=r["runtime_samples"]
        r["runtime_median_s"]=median(samples,"wall_s")
        r["runtime_cpu_median_s"]=median(samples,"cpu_s")
        r["runtime_peak_rss_mib_median"]=median(samples,"peak_rss_mib")
        r["runtime_min_s"]=min(x["wall_s"] for x in samples)
        r["runtime_max_s"]=max(x["wall_s"] for x in samples)
        r["iterations_per_second"]=N/r["runtime_median_s"]

    report = {
      "benchmark":"Swan ten-language branch + function-call workload",
      "commit":os.getenv("GITHUB_SHA","local"),
      "host":{"platform":platform.platform(),"machine":platform.machine(),"logical_cpus":os.cpu_count()},
      "workload":{"iterations":N,"operation":"if (i % 2 == 0) total += transform(i); else total += transform(i + 1);",
        "transform":"(x * 31 + 7) % 97","expected_checksum":EXPECTED,
        "compile_repeats":COMPILE_RUNS,"runtime_repeats":RUNS,"all_checksums_verified":True},
      "languages":results,
      "notes":[
        "All implementations perform the same 100M signed 64-bit iterations, parity branch, function call (except Objective-C message dispatch), multiply, add and remainder.",
        "C/C++ use native optimization flags; Rust, Go and Zig use optimized release builds.",
        "Objective-C uses GNU Objective-C runtime message dispatch on every iteration.",
        "Kotlin is Kotlin/JVM bytecode, not Kotlin/Native; runtime includes JVM startup and JIT behavior.",
        "ASM is hand-written x86-64 System V assembly and is architecture-specific.",
        "Swan compile time includes Swan assembly generation plus system assembly/linking.",
        "Compile and runtime values are medians; runtime order rotates between rounds. Every output checksum is verified.",
        "This workload tests branches, calls and integer arithmetic. It does not measure allocation, arrays, strings, concurrency, or I/O; Swan does not yet support all those features."
      ]
    }
    (OUT/"results.json").write_text(json.dumps(report,indent=2)+"\n")
    lines=["# Swan: ten-language runtime comparison","",
      f"Workload: {N:,} iterations; branch + function call + integer arithmetic; checksum {EXPECTED}.",
      "Runner: GitHub Actions ubuntu-24.04 x86-64. Lower is better.","",
      "| Language | Compile median (s) | Compile RAM (MiB) | Runtime median (s) | Runtime RAM (MiB) | Binary (B) | Iterations/s |",
      "|---|---:|---:|---:|---:|---:|---:|"]
    for name in ORDER:
        r=results[name]
        lines.append(f"| {name} | {r['compile_median_s']:.4f} | {r['compile_peak_rss_mib_median']:.2f} | {r['runtime_median_s']:.4f} | {r['runtime_peak_rss_mib_median']:.2f} | {r['artifact_bytes']:,} | {r['iterations_per_second']:.0f} |")
    lines += ["","## Methodology","- Three compilation samples and five interleaved runtime samples per language.",
      "- All binaries must print the exact same checksum; any mismatch fails the benchmark.",
      "- Swan compile time includes code generation and native assembly/linking.",
      "- Objective-C measures Objective-C message dispatch in the hot loop.",
      "- Kotlin measures the JVM process, including startup and JIT effects; it is not native AOT.",
      "- ASM is hand-written x86-64 System V and is not portable to other architectures.",
      "- This is a branch/call/integer workload, not a complete application benchmark. Arrays, allocation, strings, concurrency and I/O need separate feature-compatible suites.",
      "","Toolchain versions:"]
    lines += [f"- **{name}:** {versions[name]}" for name in ORDER]
    lines += ["","Raw samples: results.json. All checksums verified."]
    md="\n".join(lines)+"\n"
    (OUT/"results.md").write_text(md,encoding="utf-8")
    print(md)

if __name__ == "__main__":
    main()
