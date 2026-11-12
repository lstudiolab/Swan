# Compiler 2 Optimizer Benchmarks

## Current benchmark boundary

The current executable Compiler 2 slice consists of Swan-authored linear-IR
verification, i32 constant folding, SSA use-count analysis, and conservative
dead-code elimination. Compiler 1 currently compiles those Swan files into a
native object. The benchmark measures that optimizer implementation; it does
not measure a complete Compiler 2 frontend, SIL optimizer, SVM optimizer,
self-hosted compiler, target backend, or application runtime.

Do not compare this number directly with a whole-program compiler benchmark.

## Run locally

From the repository root:

~~~sh
sh build.sh
sh tests/swanc-optimizer.sh
python3 benchmarks/swanc_optimizer.py --warmups 3 --repeats 15
~~~

The benchmark accepts --compiler, --warmups, --repeats, and --output-dir.
At least three measured samples are required. Correctness checks run before
warmups and timed samples.

Reports are written to:

- benchmark-results/swanc-optimizer.md
- benchmark-results/swanc-optimizer.json

The JSON report records Git revision, host, processor, C compiler, bootstrap
executable SHA-256, workload size, warmups, all measured samples,
median/minimum/maximum/standard deviation, and throughput.

## CI

The Swan Compiler 2 Optimizer workflow runs on relevant pushes to main and can
be started manually from GitHub Actions. It builds Compiler 1 as the temporary
bootstrap, compiles the Swan-authored optimizer slice, runs correctness
regressions, and uploads reports as a 90-day artifact.

A green workflow proves only that this tested optimizer slice builds, passes
its regression cases, and completes its benchmark. It does not establish
Compiler 2 self-hosting or full language/backend support.

## Rules for adding optimizations

Every optimization must have:

1. A stated precondition and semantic invariant.
2. A positive test proving the intended transformation.
3. A negative test proving it does not transform an invalid or unsafe case.
4. Side-effect and trap-preservation tests.
5. A verifier run before and after transformation.
6. A benchmark workload that makes the pass's work visible.
7. Before/after correctness comparison using identical inputs.
8. A repeatable performance result with revision and host metadata.

Do not accept a pass merely because it improves a synthetic throughput number.
Reject transformations that change observable behavior, overflow semantics,
ownership effects, trapping behavior, or memory ordering.

## Next optimization milestones

- Replace repeated prefix scans with a single definition table and explicit
  operand/type validation in the linear verifier.
- Add basic-block-aware CFG and PHI verification before block optimizations.
- Add reusable analyses with explicit invalidation in a real pass manager.
- Add sparse conditional constant propagation and CFG simplification.
- Add dominance-aware common-subexpression elimination.
- Add loop analysis and safe loop-invariant code motion.
- Add effect-aware memory optimization.
- Implement SVM optimization only after the SVM instruction model and verifier
  are executable.
- Benchmark each pass independently and as part of the complete pipeline.
