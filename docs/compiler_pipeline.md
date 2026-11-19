<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Brew Compiler Pipeline

## One production compiler

Brew has one production compiler: Compiler 1, the C11 implementation in `compiler/tools/brewc/main.c`. `build.sh` compiles that translation unit directly into `build/brewc` and creates `build/brew` as a compatibility alias.

There is no second compiler, self-hosting compiler, bootstrap executable, generated secondary source bundle, or generated secondary object. The production compiler is not used to compile another compiler.

## User-program compilation

```text
.brew files
  -> deterministic source discovery
  -> source buffers and source identity
  -> UTF-8 validation and lexing
  -> indentation-aware parsing and AST construction
  -> shared project/module graph
  -> standard-library and C-header import loading
  -> semantic analysis and name resolution
  -> reachability analysis
  -> direct AST -> SVM typed SSA IR lowering
  -> SVM verification
  -> target-independent optimization (up to 350 pass executions)
  -> final SVM verification and analysis rebuild
  -> target-specific legalization and code generation
  -> assembly / object / WebAssembly emission
  -> native assembler and linker where applicable
```

The x86-64 AST compatibility backend remains for supported features not yet represented in typed IR, including the current array/output and runtime-lazy import paths. It is selected based on source features before typed-IR compilation. It is never used to recover from a verifier, optimizer, or code-generation failure.

## Svm optimization

SIL and SVM IR share a single compiler-wide ceiling of 350 optimization-pass executions. The current implementation consumes this budget in SIL and SVM IR; lexer, parser, semantic-analysis, machine-level, and final-emission pass registries are not yet connected. The typed-IR optimizer runs a bounded, change-driven fixed-point pass pipeline. Its current pass sequence includes constant folding, CFG simplification, algebraic simplification, copy propagation, common-subexpression elimination, loop-invariant code motion, a second CSE pass, and dead-code elimination. The pass manager stops when a complete round makes no changes and rejects failure to converge within 350 pass executions.

The compiler verifies IR at pipeline boundaries. Debug builds also verify after mutating pass boundaries. Profiling records total IR generation, IR verification, total optimization, each individual optimization pass, and target code generation. Per-pass timings are inclusive and overlap the total optimization time; they are diagnostic breakdowns, not additive totals.

## Stage profiling

Run a single compilation with profiling:

```sh
./build/brewc build --profile examples/hello.brew -o build/hello
```

The report includes source read, lexing, parsing, imports, semantic analysis, reachability, typed-IR generation, IR verification, total optimization, each optimization pass, target code generation, AST compatibility code generation, assembly output, native linking, execution where applicable, and optimization pass/round counts.

## GitHub Actions benchmark

The `Brew Compiler Pipeline Benchmark` workflow runs on compiler/benchmark changes to `main` and can also be started manually. It builds the production compiler directly, measures build time, and benchmarks generated frontend-, call-graph-, expression-, control-flow-, and runtime-heavy workloads.

Each workload uses warmups and repeated measurements. The report includes median stage durations, end-to-end wall time, compile-only peak compiler RSS (measured with --emit-asm to exclude native linking), per-header C frontend attribution from Clang `-ftime-trace`, host metadata, CSV, JSON, and Markdown output. GitHub Actions publishes the Markdown report in the run summary and uploads the raw reports as an artifact.

Run the same benchmark locally:

```sh
sh build.sh
python3 benchmarks/compiler_pipeline.py --repeats 7 --warmups 2
```

Peak RSS is reported in KiB; timing values are seconds. IR verification and per-pass rows are nested substage measurements and must not be added to their parent totals.

## Benchmark interpretation

Compare identical revisions, workloads, optimization settings, and machine classes. Keep compile time, generated-program runtime, compiler peak memory, and executable size as separate measurements. A single result does not establish a language-wide performance ranking.
