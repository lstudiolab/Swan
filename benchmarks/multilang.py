#!/usr/bin/env python3
import json, pathlib, subprocess, time

OUT = pathlib.Path("benchmark-results")
SRC, BIN = OUT / "sources", OUT / "bin"
SRC.mkdir(parents=True, exist_ok=True)
BIN.mkdir(parents=True, exist_ok=True)
N = 10_000_000
EXPECTED = N * len(b"Hello, World!\n")
programs = {
 "swan": ("hello.swan", f'fn main() -> int:\n    for i in 0..{N}\n        log "Hello, World!"\n    return 0\n'),
 "goose": ("hello.goose", f'fn main() {{\n    for i in {N} {{ print("Hello, World!"); }}\n}}\n'),
 "c": ("hello.c", f'#include <stdio.h>\nint main(void) {{ for (long i=0; i<{N}; ++i) puts("Hello, World!"); return 0; }}\n'),
 "cpp": ("hello.cpp", f'#include <cstdio>\nint main() {{ for (long i=0; i<{N}; ++i) std::puts("Hello, World!"); }}\n'),
 "objective-c": ("hello.m", f'#include <stdio.h>\n__attribute__((objc_root_class)) @interface Bench\n+ (void)run;\n@end\n@implementation Bench\n+ (void)run {{ for (long i=0; i<{N}; ++i) puts("Hello, World!"); }}\n@end\nint main(void) {{ [Bench run]; return 0; }}\n'),
 "csharp": ("Program.cs", f'using System;\nclass Program {{ static void Main() {{ for (int i=0; i<{N}; ++i) Console.WriteLine("Hello, World!"); }} }}\n')
}
for name,(filename,source) in programs.items(): (SRC/filename).write_text(source)

def rss(path):
    vals=[x.strip() for x in path.read_text().splitlines() if x.strip().isdigit()] if path.exists() else []
    return int(vals[-1]) if vals else None

def compile_run(command,label):
    mf=OUT/f"{label}-rss.txt"; mf.unlink(missing_ok=True)
    start=time.perf_counter()
    p=subprocess.run(["/usr/bin/time","-f","%M","-o",str(mf),*map(str,command)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    elapsed=time.perf_counter()-start
    if p.returncode: raise RuntimeError(f"{label} failed: {p.stderr.decode(errors='replace')}")
    return elapsed,rss(mf)

def execute(command,label):
    mf=OUT/f"{label}-rss.txt"; mf.unlink(missing_ok=True)
    start=time.perf_counter()
    p=subprocess.Popen(["/usr/bin/time","-f","%M","-o",str(mf),*map(str,command)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    count=0
    while True:
        chunk=p.stdout.read(1024*1024)
        if not chunk: break
        count+=len(chunk)
    err=p.stderr.read(); rc=p.wait(); elapsed=time.perf_counter()-start
    if rc: raise RuntimeError(f"{label} runtime failed: {err.decode(errors='replace')}")
    if count!=EXPECTED: raise RuntimeError(f"{label}: expected {EXPECTED} output bytes, got {count}")
    return elapsed,rss(mf),count

bins={k:BIN/(k+(".dll" if k=="csharp" else "")) for k in programs}
sources={k:SRC/v[0] for k,v in programs.items()}
goose=pathlib.Path("goose/build/goose").resolve()
swan=pathlib.Path("swan/build/swan").resolve()
commands={
 "swan": ([swan,"build",sources["swan"],"-o",bins["swan"]],[bins["swan"]]),
 "goose": ([goose,"build","-o",bins["goose"],sources["goose"]],[bins["goose"]]),
 "c": (["cc","-O2","-std=c11",sources["c"],"-o",bins["c"]],[bins["c"]]),
 "cpp": (["c++","-O2","-std=c++17",sources["cpp"],"-o",bins["cpp"]],[bins["cpp"]]),
 "objective-c": (["gcc","-O2","-std=gnu11","-x","objective-c",sources["objective-c"],"-lobjc","-o",bins["objective-c"]],[bins["objective-c"]])
}
results={}
for name,(build,run) in commands.items():
    ct,cm=compile_run(build,name+"-compile")
    rt,rm,ob=execute(run,name+"-runtime")
    results[name]={"compile_seconds":round(ct,4),"compile_peak_rss_kib":cm,"runtime_seconds":round(rt,4),"runtime_peak_rss_kib":rm,"source_bytes":sources[name].stat().st_size,"artifact_bytes":bins[name].stat().st_size,"output_bytes":ob,"output_mib_per_second":round(ob/1048576/rt,2),"status":"PASS"}

proj=OUT/"csharp-project"; proj.mkdir(exist_ok=True)
(proj/"Program.cs").write_text(programs["csharp"][1])
csproj=proj/"Bench.csproj"
csproj.write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><ImplicitUsings>disable</ImplicitUsings><Nullable>disable</Nullable></PropertyGroup></Project>\n')
csout=BIN/"csharp"; csout.mkdir(exist_ok=True)
ct,cm=compile_run(["dotnet","build",csproj,"-c","Release","-o",csout,"--nologo"],"csharp-compile")
dll=csout/"Bench.dll"
rt,rm,ob=execute(["dotnet",dll],"csharp-runtime")
results["csharp"]={"compile_seconds":round(ct,4),"compile_peak_rss_kib":cm,"runtime_seconds":round(rt,4),"runtime_peak_rss_kib":rm,"source_bytes":(proj/"Program.cs").stat().st_size,"artifact_bytes":dll.stat().st_size,"output_bytes":ob,"output_mib_per_second":round(ob/1048576/rt,2),"status":"PASS"}

order=["swan","goose","c","cpp","objective-c","csharp"]
data={"workload":f"{N:,} identical Hello, World! lines","expected_output_bytes":EXPECTED,"runner":"GitHub Actions ubuntu-24.04","languages":results,"notes":["Output is streamed and counted, not saved; every language must produce identical bytes.","Compile time includes native linking; C# includes dotnet build/restore.","Runtime includes output and language buffering/runtime behavior; this is not a compute-only benchmark.","C# artifact size is the managed DLL and excludes the shared .NET runtime.","Objective-C uses GCC's Objective-C frontend, GNU libobjc, a root class, and one message dispatch."]}
(OUT/"results.json").write_text(json.dumps(data,indent=2)+"\n")
def mib(k): return "N/A" if k is None else f"{k/1024:.2f}"
lines=["# Swan, Goose, C, C++, Objective-C and C# benchmark","",f"Workload: {N:,} lines; expected output: {EXPECTED:,} bytes.","Runner: GitHub Actions ubuntu-24.04. Lower values are better.","","| Language | Compile (s) | Compile RAM (MiB) | Runtime (s) | Runtime RAM (MiB) | Source (B) | Artifact (B) | Output (B) | Throughput (MiB/s) |","|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
for name in order:
 r=results[name]
 lines.append(f"| {name} | {r['compile_seconds']:.2f} | {mib(r['compile_peak_rss_kib'])} | {r['runtime_seconds']:.2f} | {mib(r['runtime_peak_rss_kib'])} | {r['source_bytes']:,} | {r['artifact_bytes']:,} | {r['output_bytes']:,} | {r['output_mib_per_second']:.2f} |")
lines += ["","## Method and limitations","- Every program must emit the same output bytes or the benchmark fails.","- Output is streamed to a byte counter, not retained in memory or written to disk.","- Runtime includes loop execution, output calls, and each language's buffering/runtime behavior; it is not a compute-only comparison.","- C# artifact size is its managed DLL, excluding the shared .NET runtime; other artifacts are native executables.","- Peak RAM is maximum resident set size (RSS) for the measured process.","","Raw measurements: results.json."]
(OUT/"results.md").write_text("\n".join(lines)+"\n")
print("\n".join(lines))
