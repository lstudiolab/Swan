#!/usr/bin/env python3
"""Identical 20M-element / 20M-query binary search benchmark: Swan, Goose, C, Rust."""
import json, os, pathlib, platform, statistics, subprocess, tempfile, time

ROOT = pathlib.Path(__file__).resolve().parents[1]
N = 20_000_000
EXPECTED = N * (N - 1) // 2
ROUNDS = 5
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
  while(low<=high) {
   int64_t mid=low+(high-low)/2; int64_t value=(int64_t)a[mid];
   if(value==target){found=value;break;}
   if(value<target)low=mid+1;else high=mid-1;
  }
  sum+=found;target+=7919;if(target>=N)target-=N;
 }
 printf("%" PRId64 "\n",sum); free(a); return 0;
}
"""
RUST = r"""
const N:i64=20_000_000;
fn main() {
 let mut a=vec![0_u32;N as usize];
 for i in 0..N { a[i as usize]=i as u32; }
 let mut sum=0_i64;let mut target=0_i64;
 for _ in 0..N {
  let(mut low,mut high,mut found)=(0_i64,N-1,-1_i64);
  while low<=high {
   let mid=low+(high-low)/2;let value=a[mid as usize] as i64;
   if value==target {found=value;break;}
   if value<target {low=mid+1;}else{high=mid-1;}
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
   if value==target {found=value;break;}
   if value<target {low=middle+1;}else{high=middle-1;}
  }
  sum+=found;target+=7919;if target>=N{target-=N;}
 }
 print(sum);
}
"""
SWAN_SRC = r"""
fn main() -> int:
    let values = array(20000000)
    var i = 0
    while i < 20000000
        values[i] = i
        i = i + 1
    var query = 0
    var checksum = 0
    var target = 0
    while query < 20000000
        var low = 0
        var high = 19999999
        var found = -1
        while low <= high
            var middle = low + (high - low) // 2
            var value = values[middle]
            if value == target
                found = value
                break
            else
                if value < target
                    low = middle + 1
                else
                    high = middle - 1
        checksum = checksum + found
        target = target + 7919
        if target >= 20000000
            target = target - 20000000
        query = query + 1
    log checksum
    return 0
"""

def timed(command, cwd):
    start=time.perf_counter()
    p=subprocess.run(command,cwd=cwd,text=True,capture_output=True)
    wall=time.perf_counter()-start
    if p.returncode:
        raise RuntimeError(f"Command failed ({p.returncode}): {command}\n{p.stdout}\n{p.stderr}")
    return wall,p.stdout.strip()

def measured(command, cwd):
    with tempfile.NamedTemporaryFile(mode="w+",delete=False) as f: timing=pathlib.Path(f.name)
    p=subprocess.run(["/usr/bin/time","-f","%e %U %S %M","-o",str(timing),*map(str,command)],
                     cwd=cwd,text=True,capture_output=True)
    raw=timing.read_text().strip().split();timing.unlink(missing_ok=True)
    if p.returncode: raise RuntimeError(f"Runtime failed ({p.returncode}): {command}\n{p.stdout}\n{p.stderr}")
    if len(raw)!=4: raise RuntimeError(f"GNU time output invalid: {raw}")
    wall,user,system,rss=map(float,raw)
    output=p.stdout.strip()
    if output.splitlines()[-1] != str(EXPECTED):
        raise RuntimeError(f"Checksum mismatch: expected {EXPECTED}, got {output!r}")
    return {"wall_s":wall,"cpu_s":user+system,"peak_rss_mib":rss/1024}

def main():
    compiler=ROOT/"build/swan"
    if not compiler.is_file(): raise SystemExit("Build Swan first: sh build.sh")
    with tempfile.TemporaryDirectory(prefix="swan-four-way-") as td:
        d=pathlib.Path(td)
        sources={
          "Swan":(d/"search.swan",SWAN_SRC),
          "Goose":(d/"search.goose",GOOSE_SRC),
          "C":(d/"search.c",C),
          "Rust":(d/"search.rs",RUST),
        }
        for path,body in sources.values(): path.write_text(body)
        bins={name:d/("bin-"+name.lower()) for name in sources}
        commands={
          "Swan":[str(compiler),"build",str(sources["Swan"][0]),"-o",str(bins["Swan"])],
          "Goose":[GOOSE,"build","-o",str(bins["Goose"]),str(sources["Goose"][0])],
          "C":["cc","-O3","-march=native","-DNDEBUG",str(sources["C"][0]),"-o",str(bins["C"])],
          "Rust":["rustc","-C","opt-level=3","-C","target-cpu=native",str(sources["Rust"][0]),"-o",str(bins["Rust"])],
        }
        names=["Swan","Goose","C","Rust"]
        versions={
          "Swan":"repository main",
          "Goose":subprocess.run([GOOSE,"version"],text=True,capture_output=True).stdout.strip().splitlines()[0],
          "C":subprocess.run(["cc","--version"],text=True,capture_output=True).stdout.splitlines()[0],
          "Rust":subprocess.run(["rustc","--version"],text=True,capture_output=True).stdout.strip(),
        }
        results={}
        for name in names:
            print("COMPILING "+name,flush=True)
            seconds,_=timed(commands[name],ROOT)
            results[name]={"compile_seconds":round(seconds,3),"binary_bytes":bins[name].stat().st_size,"runtime_samples":[]}
        print("WARMUP AND CHECKSUM VALIDATION",flush=True)
        for name in names:
            sample=measured([str(bins[name])],ROOT)
            results[name]["warmup_checksum"]=EXPECTED
        for r in range(ROUNDS):
            order=names[r:]+names[:r]
            print(f"RUNTIME ROUND {r+1}/{ROUNDS}: {', '.join(order)}",flush=True)
            for name in order:
                sample=measured([str(bins[name])],ROOT)
                results[name]["runtime_samples"].append(sample)
        for name in names:
            samples=results[name]["runtime_samples"]
            for key in ("wall_s","cpu_s","peak_rss_mib"):
                results[name]["runtime_"+key]=round(statistics.median(x[key] for x in samples),3)
        report={
          "workload":"20,000,000 uint32 values; 20,000,000 binary searches; target advances by 7919 modulo 20,000,000",
          "array_elements":N,"queries":N,"expected_checksum":EXPECTED,
          "rounds":ROUNDS,"measurement_order":"rotated order per round",
          "machine":{"platform":platform.platform(),"architecture":platform.machine(),"cpu_count":os.cpu_count()},
          "toolchains":versions,"results":results
        }
        (ROOT/"four-way-binary-search.json").write_text(json.dumps(report,indent=2)+"\n")
        print("\n| Language | Compile (s) | Runtime wall (s) | Runtime CPU (s) | Peak RSS (MiB) | Binary (B) |")
        print("|---|---:|---:|---:|---:|---:|")
        for name in names:
            x=results[name]
            print(f"| {name} | {x['compile_seconds']:.3f} | {x['runtime_wall_s']:.3f} | {x['runtime_cpu_s']:.3f} | {x['runtime_peak_rss_mib']:.2f} | {x['binary_bytes']} |")
        print(f"\nChecksum verified for all four: {EXPECTED}")
        print("Runtime values are medians of five measured runs, in seconds.")

if __name__=="__main__": main()
