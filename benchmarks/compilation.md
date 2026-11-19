<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Compilation benchmarks

## Heavy six-toolchain comparison

Use `compile_speed.py` to compare source-to-native-executable compile time for:

- Goose
- Brew
- Rust
- C++
- C
- x86-64 GNU assembler (ASM)

The benchmark generates one large translation unit for each high-level language.
Each contains hundreds of reachable functions, with a configurable chain of
dependent arithmetic operations per function. The default workload is 384
functions × 96 operations (36,864 dependent arithmetic statements per language),
plus a call site for every generated function.

It records median and best compile-plus-link wall time, peak process RSS,
source size, binary size, and toolchain version. Each measured run gets a
unique source and output path to avoid incremental-build cache hits. Warmups
are excluded and measurement order rotates between languages.

### Run

```sh
sh build.sh
GOOSE_BIN=/path/to/goose python3 benchmarks/compile_speed.py \
  --repeats 5 --warmups 1 --functions 384 --operations 96
```

The Brew compiler defaults to `build/brew`. Override it with `--brew` or
`BREW_BIN`. Override the Goose compiler with `--goose` or `GOOSE_BIN`.
C and C++ compilers can be selected with `--cc` and `--cxx`. Rust uses
`rustc`.

By default all six toolchains are required. `--allow-missing` permits a
partial local run, but partial results must not be presented as a six-way
comparison. Brew and Goose remain required.

### Fairness and interpretation

- The primary metric is cold source-to-executable time, including linking.
- Rust, C++, and C use `-O2`; Brew and Goose use their normal native build
  pipelines. Record the exact compiler versions printed in the report.
- The same workload shape and constants are generated for Goose, Brew, Rust,
  C++, and C. Arithmetic is data-dependent within each function so the source
  exercises parsing, semantic analysis, optimization, and code generation.
- The ASM row uses handwritten x86-64 GAS and measures assembly plus linking.
  It has no high-level frontend or equivalent optimizer, so it is a low-level
  toolchain reference rather than a directly equivalent language comparison.
- Runtime is deliberately excluded. This benchmark measures compilation, not
  generated program execution speed.
- Run on an otherwise idle machine. Compare medians from the same host,
  operating system, compiler versions, and flags. Do not treat results from
  different machines as a language ranking.

The report is written to `benchmark-results/compile-speed.md` by default.
