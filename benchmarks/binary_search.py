#!/usr/bin/env python3
"""20M-element, 20M-query zero-initialized binary-search benchmark with checksum checks."""
from __future__ import annotations
import json, os, pathlib, statistics, subprocess, tempfile, time

ROOT = pathlib.Path(__file__).resolve().parents[1]
N = 20_000_000
RUNS = 5
EXPECTED = N * (N - 1) // 2
GOOSE = os.environ.get("GOOSE", "goose")
TIME = "/usr/bin/time"

C = r"""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define N 20000000LL
int main(void) {
 uint32_t *a = calloc((size_t)N, sizeof(*a)); if (!a) return 2;
 for (int64_t i=0;i<N;i++) a[i]=i;
 int64_t sum=0;
 for (int64_t q=0;q<N;q++) {
  int64_t target=(q*7919)%N, low=0, high=N-1, found=-1;
  while(low<=high) { int64_t mid=low+(high-low)/2, v=a[mid];
   int64_t value=(int64_t)a[mid];
   if(value==target){found=value;break;} if(value<target)low=mid+1;else high=mid-1;
  } sum+=found;
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
 for(int64_t i=0;i<n;i++)a[(size_t)i]=i;
 int64_t sum=0;
 for(int64_t q=0;q<n;q++){
  int64_t target=(q*7919)%n,low=0,high=n-1,found=-1;
  while(low<=high){int64_t mid=low+(high-low)/2,v=a[(size_t)mid];
   int64_t value=(int64_t)v; if(value==target){found=value;break;}if(value<target)low=mid+1;else high=mid-1;
  }sum+=found;
 }
 std::printf("%lld\n",(long long)sum);
}
"""
RUST = r"""
const N:i64=20_000_000;
fn main(){
 let mut a=vec![0_u32;N as usize];
 for i in 0..N{a[i as usize]=i as u32;}
 let mut sum=0_i64;
 for q in 0..N{
  let target=(q*7919)%N;let(mut low,mut high,mut found)=(0_i64,N-1,-1_i64);
  while low<=high{let mid=low+(high-low)/2;let v=a[mid as usize];
   let value=v as i64; if value==target{found=value;break;}if value<target{low=mid+1;}else{high=mid-1;}
  }sum+=found;
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
 var sum:i64=0;
 for query in 0..N {
  let target=(query*7919)%N;
  var low=0; var high=N-1; var found=-1;
  while low<=high {
   let middle=low+(high-low)/2;
   let value=values[middle] as i64;
   if value==target { found=value; break; }
   if value<target { low=middle+1; } else { high=middle-1; }
  }
  sum+=found;
 }
 print(sum);
}
"""

def run(command, extra_env=None):
    with tempfile.NamedTemporaryFile(mode="w+", delete=False) as f: rss=pathlib.Path(f.name)
    env=os.environ.copy()
    if extra_env: env.update(extra_env)
    start=time.perf_counter()
    p=subprocess.run([TIME,"-f","%M","-o",str(rss),*command],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env)
    elapsed=time.perf_counter()-start
    rss_lines=rss.read_text().strip().splitlines()
    peak=int(rss_lines[-1] if rss_lines else "0")/1024
    rss.unlink(missing_ok=True)
    if p.returncode: raise RuntimeError(f"{command} failed (exit {p.returncode})\n{p.stdout}\n{p.stderr}")
    return elapsed,peak,p.stdout.strip()

def main():
 with tempfile.TemporaryDirectory(prefix="swan-bsearch-") as td:
  d=pathlib.Path(td)
  src={}
  for name,body,suffix in [("C",C,"c"),("C++",CPP,"cpp"),("Rust",RUST,"rs"),("Goose",GOOSE_SRC,"goose")]:
   src[name]=d/f"search.{suffix}";src[name].write_text(body)
  exe={name:d/name.lower().replace("+","p") for name in src}
  exe["Swan"]=d/"swan-search"
  commands={
   "C":["cc","-O3","-march=native","-DNDEBUG",str(src["C"]),"-o",str(exe["C"])],
   "C++":["c++","-O3","-march=native","-DNDEBUG","-std=c++17",str(src["C++"]),"-o",str(exe["C++"])],
   "Rust":["rustc","-C","opt-level=3","-C","target-cpu=native",str(src["Rust"]),"-o",str(exe["Rust"])],
   "Goose":[GOOSE,"build","-o",str(exe["Goose"]),str(src["Goose"])],
   "Swan":[str(ROOT/"build/swan"),"build",str(ROOT/"benchmarks/binary_search.swan"),"-o",str(exe["Swan"])],
  }
  results={}
  names=("Swan","Goose","Rust","C","C++")
  for name in names:
   print(f"COMPILING {name}",flush=True)
   ct,cm,_=run(commands[name])
   if not exe[name].is_file(): raise RuntimeError(f"{name} executable missing")
   results[name]={"compile_seconds":round(ct,3),"compile_peak_mib":round(cm,2),
                  "runtime_samples_seconds":[],"runtime_samples_peak_mib":[],
                  "checksum":EXPECTED,"executable_bytes":exe[name].stat().st_size}
  # Interleave implementations and rotate the starting point each round to
  # reduce fixed-order CPU temperature, frequency and background-load bias.
  for attempt in range(RUNS):
   order=names[attempt:]+names[:attempt]
   print(f"MEASUREMENT ROUND {attempt+1}/{RUNS}: {', '.join(order)}",flush=True)
   for name in order:
    rt,rm,out=run([str(exe[name])])
    try: checksum=int(out.splitlines()[-1])
    except Exception: raise RuntimeError(f"{name} invalid output: {out!r}")
    if checksum!=EXPECTED: raise RuntimeError(f"{name} checksum {checksum}; expected {EXPECTED}")
    results[name]["runtime_samples_seconds"].append(round(rt,3))
    results[name]["runtime_samples_peak_mib"].append(round(rm,2))
  for name in names:
   samples=results[name]["runtime_samples_seconds"]
   results[name]["runtime_seconds"]=round(statistics.median(samples),3)
   results[name]["runtime_peak_mib"]=round(statistics.median(results[name]["runtime_samples_peak_mib"]),2)
   print(f"PASS {name}: compile {results[name]['compile_seconds']:.3f}s, "
         f"median runtime {results[name]['runtime_seconds']:.3f}s, samples {samples}, "
         f"peak RSS {results[name]['runtime_peak_mib']:.2f} MiB",flush=True)
  report={"array_elements":N,"queries":N,"query_formula":"(q * 7919) % 20000000","expected_checksum":EXPECTED,"all_checksums_valid":True,"languages":results}
  out=ROOT/"binary-search-benchmark.json";out.write_text(json.dumps(report,indent=2)+"\n")
  print("\n| Language | Compile s | Compile RAM MiB | Runtime s | Runtime RAM MiB | Binary B |")
  print("|---|---:|---:|---:|---:|---:|")
  for name,x in results.items(): print(f"| {name} | {x['compile_seconds']:.3f} | {x['compile_peak_mib']:.2f} | {x['runtime_seconds']:.3f} | {x['runtime_peak_mib']:.2f} | {x['executable_bytes']} |")
  print(f"\nChecksum verified: {EXPECTED}")
if __name__=="__main__": main()
