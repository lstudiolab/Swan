#!/usr/bin/env python3
"""Reproducible microbenchmark for Swan-authored Compiler 2 optimizer code.

Compiler 1 currently bootstraps this Swan source. Timed work covers linear-IR
verification, i32 constant folding, use counts, and conservative DCE only.
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, platform, statistics, subprocess, tempfile, time

ROOT=pathlib.Path(__file__).resolve().parents[1]
DEFAULT_COMPILER=ROOT/"build"/"swan"
INSTRUCTIONS_PER_SAMPLE=515*128
FOLDS_PER_SAMPLE=128*128
DCE_REMOVALS_PER_SAMPLE=128

def run(command, cwd=ROOT):
    return subprocess.run(command,cwd=cwd,text=True,capture_output=True)

def require(result,label):
    if result.returncode:
        raise RuntimeError(f"{label} failed (exit {result.returncode}):\n{(result.stderr or result.stdout)[-16000:]}")

def make_source(path):
    parts=["swanc/ir/IR.swan","swanc/optimization/ConstantFold.swan",
           "swanc/optimization/UseCountAnalysis.swan","swanc/optimization/DeadCodeElimination.swan",
           "swanc/optimization/Optimization.swan"]
    source="\n".join((ROOT/p).read_text(encoding="utf-8") for p in parts)
    source += "\nfn main() -> int:\n    return swanc_ir_optimize(array_unchecked(8), 0, array_unchecked(4), 1)\nout\n"
    path.write_text(source,encoding="utf-8")
    return path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler",type=pathlib.Path,default=DEFAULT_COMPILER)
    parser.add_argument("--warmups",type=int,default=3)
    parser.add_argument("--repeats",type=int,default=15)
    parser.add_argument("--output-dir",type=pathlib.Path,default=ROOT/"benchmark-results")
    args=parser.parse_args()
    if args.warmups<0: parser.error("--warmups cannot be negative")
    if args.repeats<3: parser.error("--repeats must be at least 3")
    compiler=args.compiler.resolve()
    if not compiler.is_file(): raise SystemExit(f"Compiler not found: {compiler}; run sh build.sh first.")
    with tempfile.TemporaryDirectory(prefix="swanc-optimizer-") as temp:
        directory=pathlib.Path(temp); source=make_source(directory/"optimizer.swan")
        assembly=directory/"optimizer.s"; executable=directory/"optimizer-test"
        start=time.perf_counter_ns()
        emitted=run([str(compiler),str(source),"--target","x86_64","--emit-asm","-o",str(assembly)])
        require(emitted,"Compiler 1 bootstrap of Compiler 2 optimizer source")
        renamed=run(["sed","-i","-E",r"s/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_optimizer_module_stub\2/g",str(assembly)])
        require(renamed,"generated entry-point rename")
        linked=run(["cc","-O2",str(assembly),str(ROOT/"tests/swanc_optimizer.c"),"-o",str(executable)])
        require(linked,"native optimizer harness link")
        build_seconds=(time.perf_counter_ns()-start)/1e9
        check=run([str(executable)]); require(check,"optimizer correctness suite")
        warmups=[]
        for _ in range(args.warmups):
            start=time.perf_counter_ns(); sample=run([str(executable)])
            elapsed=(time.perf_counter_ns()-start)/1e9; require(sample,"optimizer warmup"); warmups.append(elapsed)
        samples=[]
        for _ in range(args.repeats):
            start=time.perf_counter_ns(); sample=run([str(executable)])
            elapsed=(time.perf_counter_ns()-start)/1e9; require(sample,"optimizer timed sample"); samples.append(elapsed)
    median=statistics.median(samples)
    version=run(["cc","--version"])
    revision=run(["git","rev-parse","HEAD"])
    result={
      "benchmark":"swanc-optimizer-microbenchmark",
      "git_revision":revision.stdout.strip() if revision.returncode==0 else "unknown",
      "host":platform.platform(),"machine":platform.machine(),
      "processor":platform.processor() or "not reported","python":platform.python_version(),
      "c_compiler":(version.stdout or version.stderr).splitlines()[0],
      "compiler_executable":str(compiler),
      "compiler_sha256":hashlib.sha256(compiler.read_bytes()).hexdigest(),
      "compiler_generation":"Compiler 1 bootstraps Swan-authored Compiler 2 optimizer",
      "scope":"linear IR verification, i32 constant folding, use-count analysis, conservative DCE",
      "not_measured":["full Compiler 2 compilation","self-hosting","SIL/SVM optimization","backend code generation","application runtime"],
      "ir_instructions_per_sample":INSTRUCTIONS_PER_SAMPLE,
      "constant_folds_per_sample":FOLDS_PER_SAMPLE,
      "dead_instructions_removed_per_sample":DCE_REMOVALS_PER_SAMPLE,
      "warmups":args.warmups,"repeats":args.repeats,
      "optimizer_module_compile_and_link_seconds":build_seconds,
      "warmup_samples_seconds":warmups,"runtime_samples_seconds":samples,
      "runtime_min_seconds":min(samples),"runtime_median_seconds":median,
      "runtime_max_seconds":max(samples),"runtime_stdev_seconds":statistics.stdev(samples),
      "ir_instructions_per_second":INSTRUCTIONS_PER_SAMPLE/median,
      "constant_folds_per_second":FOLDS_PER_SAMPLE/median,
      "dead_instructions_removed_per_second":DCE_REMOVALS_PER_SAMPLE/median,
      "correctness_output":check.stdout.strip()
    }
    out=args.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
    (out/"swanc-optimizer.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    report=[
      "# swanc optimizer microbenchmark","",
      f"- Revision: {result['git_revision']}",
      f"- Host: {result['host']} ({result['machine']})",
      f"- Processor: {result['processor']}",
      f"- C compiler: {result['c_compiler']}",
      f"- Bootstrap executable SHA-256: {result['compiler_sha256']}",
      f"- Execution: {result['compiler_generation']}",
      f"- Scope: {result['scope']}",
      f"- IR instructions/sample: {INSTRUCTIONS_PER_SAMPLE:,}",
      f"- Constant folds/sample: {FOLDS_PER_SAMPLE:,}",
      f"- Dead instructions removed/sample: {DCE_REMOVALS_PER_SAMPLE:,}",
      f"- Module compile + link: {build_seconds:.9f} s",
      f"- Warmups / samples: {args.warmups} / {args.repeats}",
      f"- Runtime median: {median:.9f} s",
      f"- Runtime min / max: {min(samples):.9f} / {max(samples):.9f} s",
      f"- Runtime standard deviation: {statistics.stdev(samples):.9f} s",
      f"- IR throughput: {result['ir_instructions_per_second']:.2f} instructions/s",
      f"- Fold throughput: {result['constant_folds_per_second']:.2f} folds/s",
      f"- DCE throughput: {result['dead_instructions_removed_per_second']:.2f} removals/s","",
      "**Scope warning:** Compiler 1 bootstraps this Swan-authored optimizer slice. This is not a whole-compiler or self-hosting benchmark.","",
      "## Correctness","",result["correctness_output"],"","## Samples","",
      ", ".join(f"{x:.9f}" for x in samples)]
    (out/"swanc-optimizer.md").write_text("\n".join(report)+"\n",encoding="utf-8")
    print("\n".join(report)); print(f"JSON report: {out/'swanc-optimizer.json'}")

if __name__=="__main__": main()
