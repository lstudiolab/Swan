#!/usr/bin/env python3
"""Balanced 20M-element / 20M-query binary-search comparison."""
from __future__ import annotations
import json, os, pathlib, statistics, subprocess, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
N = 20_000_000
RUNS = 6
EXPECTED = N * (N - 1) // 2
GOOSE = os.environ.get("GOOSE", "goose")

C = r"""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define N 20000000LL
int main(void) {
 uint32_t *a = calloc((size_t)N, sizeof(*a)); if (!a) return 2;
 for (int64_t i=0;i<N;i++) a[i]=(uint32_t)i;
 int64_t sum=0,target=0;
 for (int64_t q=0;q<N;q++) {
  int64_t low=0,high=N-1,found=-1;
  while(low<=high) { int64_t mid=low+(high-low)/2; int64_t value=(int64_t)a[mid];
   if(value==target){found=value;break;} if(value<target)low=mid+1;else high=mid-1;
  }
  sum+=found;target+=7919;if(target>=N)target-=N;
 }
 printf("%" PRId64 "\n",sum); free(a); return 0;
}
"""
CPP = r"""
#include <cstdint>
#include <cstdio>
#include <vector>
int main() {
 constexpr int64_t n=20000000; std::vector<uint32_t>a((size_t)n);
 for(int64_t i=0;i<n;i++)a[(size_t)i]=(uint32_t)i;
 int64_t sum=0,target=0;
 for(int64_t q=0;q<n;q++){
  int64_t low=0,high=n-1,found=-1;
  while(low<=high){int64_t mid=low+(high-low)/2;int64_t value=(int64_t)a[(size_t)mid];
   if(value==target){found=value;break;}if(value<target)low=mid+1;else high=mid-1;
  }
  sum+=found;target+=7919;if(target>=n)target-=n;
 }
 std::printf("%lld\n",(long long)sum);
}
"""
RUST = r"""
const N:i64=20_000_000;
fn main(){
 let mut a=vec![0_u32;N as usize];
 for i in 0..N{a[i as usize]=i as u32;}
 let mut sum=0_i64;let mut target=0_i64;
 for _q in 0..N{
  let(mut low,mut high,mut found)=(0_i64,N-1,-1_i64);
  while low<=high{let mid=low+(high-low)/2;let value=a[mid as usize] as i64;
   if value==target{found=value;break;}if value<target{low=mid+1;}else{high=mid-1;}
  }
  sum+=found;target+=7919;if target>=N{target-=N;}
 }
 println!("{}",std::hint::black_box(sum));
}
"""
GOOSE_SRC = r"""
import std;
let N = 20000000;
fn main() {
 var values:u32[]=[0;N];
 for i in 0..N { values[i]=i as u32; }
 var sum:i64=0;var target=0;
 for query in 0..N {
  var low=0;var high=N-1;var found=-1;
  while low<=high {
   let middle=low+(high-low)/2;
   let value=values[middle] as i64;
   if value==target { found=value; break; }
   if value<target { low=middle+1; } else { high=middle-1; }
  }
  sum+=found;target+=7919;if target>=N{target-=N;}
 }
 print(sum);
}
"""
GO = r"""
package main
import "fmt"
const n int64 = 20000000
func main() {
 a:=make([]uint32,n)
 for i:=int64(0);i<n;i++ { a[i]=uint32(i) }
 var sum,target int64
 for q:=int64(0);q<n;q++ {
  low,high,found:=int64(0),n-1,int64(-1)
  for low<=high {
   mid:=low+(high-low)/2
   value:=int64(a[mid])
   if value==target { found=value;break }
   if value<target { low=mid+1 } else { high=mid-1 }
  }
  sum+=found;target+=7919;if target>=n { target-=n }
 }
 fmt.Println(sum)
}
"""
ZIG = r"""
const std = @import("std");
const N: i64 = 20_000_000;
pub fn main() !void {
    const allocator = std.heap.page_allocator;
    const values = try allocator.alloc(u32, @intCast(N));
    defer allocator.free(values);
    @memset(values, 0);
    for (values, 0..) |*value, i| value.* = @intCast(i);
    var sum: i64 = 0;
    var target: i64 = 0;
    var query: i64 = 0;
    while (query < N) : (query += 1) {
        var low: i64 = 0;
        var high: i64 = N - 1;
        var found: i64 = -1;
        while (low <= high) {
            const middle = low + @divTrunc(high - low, 2);
            const value: i64 = @intCast(values[@intCast(middle)]);
            if (value == target) { found = value; break; }
            if (value < target) { low = middle + 1; } else { high = middle - 1; }
        }
        sum += found;
        target += 7919;
        if (target >= N) target -= N;
    }
    const stdout = std.io.getStdOut().writer();
    try stdout.print("{d}\n", .{sum});
}
"""

def run(command):
    with tempfile.NamedTemporaryFile(mode="w+", delete=False) as f:
        timing = pathlib.Path(f.name)
    p = subprocess.run(["/usr/bin/time", "-f", "%e %U %S %M", "-o", str(timing), *command],
                       cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    raw = timing.read_text().strip().split()
    timing.unlink(missing_ok=True)
    if p.returncode:
        raise RuntimeError(f"{command} failed (exit {p.returncode})\n{p.stdout}\n{p.stderr}")
    if len(raw) != 4:
        raise RuntimeError(f"could not parse GNU time output: {raw!r}")
    wall, user, system, peak = map(float, raw)
    return wall, user + system, peak / 1024, p.stdout.strip()

def check_output(name, output):
    try:
        value = int(output.splitlines()[-1])
    except Exception:
        raise RuntimeError(f"{name} invalid output: {output!r}")
    if value != EXPECTED:
        raise RuntimeError(f"{name} checksum {value}; expected {EXPECTED}")

def main():
    with tempfile.TemporaryDirectory(prefix="swan-bsearch-") as td:
        d = pathlib.Path(td)
        bodies = {"C": (C, "c"), "C++": (CPP, "cpp"), "Rust": (RUST, "rs"),
                  "Goose": (GOOSE_SRC, "goose"), "Go": (GO, "go"), "Zig": (ZIG, "zig")}
        src = {}
        for name, (body, suffix) in bodies.items():
            src[name] = d / f"search.{suffix}"
            src[name].write_text(body)
        exe = {name: d / name.lower().replace("+", "p") for name in bodies}
        exe["Swan"] = d / "swan-checked"
        exe["Swan unchecked"] = d / "swan-unchecked"
        commands = {
            "C": ["cc", "-O3", "-march=native", "-DNDEBUG", str(src["C"]), "-o", str(exe["C"])],
            "C++": ["c++", "-O3", "-march=native", "-DNDEBUG", "-std=c++17", str(src["C++"]), "-o", str(exe["C++"])],
            "Rust": ["rustc", "-C", "opt-level=3", "-C", "target-cpu=native", str(src["Rust"]), "-o", str(exe["Rust"])],
            "Goose": [GOOSE, "build", "-o", str(exe["Goose"]), str(src["Goose"])],
            "Go": ["go", "build", "-trimpath", "-ldflags=-s -w", "-o", str(exe["Go"]), str(src["Go"])],
            "Zig": ["zig", "build-exe", "-O", "ReleaseFast", "-mcpu=native", str(src["Zig"]), "-femit-bin=" + str(exe["Zig"])],
            "Swan": [str(ROOT / "build/swan"), "build", str(ROOT / "benchmarks/binary_search.swan"), "-o", str(exe["Swan"])],
            "Swan unchecked": [str(ROOT / "build/swan"), "build", str(ROOT / "benchmarks/binary_search_unchecked.swan"), "-o", str(exe["Swan unchecked"])],
        }
        names = ("Swan", "Swan unchecked", "Goose", "Rust", "C", "C++", "Go", "Zig")
        assembly_paths = {
            "Swan": ROOT / "binary-search-swan-checked.s",
            "Swan unchecked": ROOT / "binary-search-swan-unchecked.s",
        }
        for name, source in [("Swan", "binary_search.swan"), ("Swan unchecked", "binary_search_unchecked.swan")]:
            subprocess.run([str(ROOT / "build/swan"), str(ROOT / "benchmarks" / source),
                            "--emit-asm", "-o", str(assembly_paths[name])], cwd=ROOT, check=True)
        results = {}
        for name in names:
            print(f"COMPILING {name}", flush=True)
            ct, ccpu, cm, _ = run(commands[name])
            if not exe[name].is_file():
                raise RuntimeError(f"{name} executable missing")
            results[name] = {"compile_seconds": round(ct, 3), "compile_cpu_seconds": round(ccpu, 3),
                             "compile_peak_mib": round(cm, 2), "runtime_samples_seconds": [],
                             "runtime_samples_cpu_seconds": [], "runtime_samples_peak_mib": [],
                             "checksum": EXPECTED, "executable_bytes": exe[name].stat().st_size}
        print("WARMUP: validating each executable before measurement", flush=True)
        for name in names:
            _, _, _, output = run([str(exe[name])])
            check_output(name, output)
        orders = []
        for attempt in range(RUNS):
            order = names[attempt:] + names[:attempt]
            orders.append(list(order))
            print(f"MEASUREMENT ROUND {attempt+1}/{RUNS}: {', '.join(order)}", flush=True)
            for name in order:
                wall, cpu, peak, output = run([str(exe[name])])
                check_output(name, output)
                results[name]["runtime_samples_seconds"].append(round(wall, 3))
                results[name]["runtime_samples_cpu_seconds"].append(round(cpu, 3))
                results[name]["runtime_samples_peak_mib"].append(round(peak, 2))
        for name in names:
            x = results[name]
            x["runtime_seconds"] = round(statistics.median(x["runtime_samples_seconds"]), 3)
            x["runtime_cpu_seconds"] = round(statistics.median(x["runtime_samples_cpu_seconds"]), 3)
            x["runtime_peak_mib"] = round(statistics.median(x["runtime_samples_peak_mib"]), 2)
            print(f"PASS {name}: compile {x['compile_seconds']:.3f}s, "
                  f"runtime wall {x['runtime_seconds']:.3f}s, CPU {x['runtime_cpu_seconds']:.3f}s, "
                  f"RSS {x['runtime_peak_mib']:.2f} MiB, samples {x['runtime_samples_seconds']}", flush=True)
        report = {"array_elements": N, "queries": N,
                  "query_formula": "target starts at 0 and advances by 7919 modulo 20000000",
                  "array_modes": {"Swan": "bounds checked", "Swan unchecked": "unchecked"},
                  "measurement_rounds": RUNS, "balanced_measurement_orders": orders,
                  "expected_checksum": EXPECTED, "all_checksums_valid": True, "languages": results}
        out = ROOT / "binary-search-benchmark.json"
        out.write_text(json.dumps(report, indent=2) + "\n")
        print("\n| Language | Compile s | Runtime wall s | Runtime CPU s | Peak RSS MiB | Binary B |")
        print("|---|---:|---:|---:|---:|---:|")
        for name, x in results.items():
            print(f"| {name} | {x['compile_seconds']:.3f} | {x['runtime_seconds']:.3f} | {x['runtime_cpu_seconds']:.3f} | {x['runtime_peak_mib']:.2f} | {x['executable_bytes']} |")
        print(f"\nChecksum verified: {EXPECTED}")

if __name__ == "__main__":
    main()
