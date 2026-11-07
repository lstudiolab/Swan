# Swan performance benchmarks

## Native backend optimization levels

Run the focused backend experiment after building Swan and installing Clang:

```sh
sh build.sh
python3 benchmarks/backend_flags.py
```

The harness generates one Swan program, compiles the resulting C with Clang
at `-O0`, `-O1`, `-O2`, `-O3`, and `-Os`, and compares median backend
compile time with median runtime. It verifies that every executable produces
the same checksum. This isolates the native C backend from Swan's own
lexer/parser/sema/codegen time.

On the recorded Ubuntu 26.04 / Clang 21.1.8 runner, `-O1` compiled in
52.25 ms versus 53.61 ms for `-O2`; runtime was 129.83 ms versus 129.95 ms.
That makes `-O1` the recommended fast-build setting for this workload. The
difference is small, so treat it as a measured default, not a universal
speedup guarantee.

## Cross-language baseline

Run after building Swan and installing Clang, Rust, Goose, and GNU time:

```sh
sh build.sh
python3 benchmarks/compare_languages.py
```

The harness runs equivalent 50-million-iteration scalar loops in Swan, Rust,
and Goose. Swan and Goose emit C, which is compiled with Clang. Rust uses rustc
with optimization level 3 and the native CPU target. It reports end-to-end
compile-pipeline wall time, peak RSS for the largest individual
compiler/backend process, median native execution time after one warm-up, and
peak RSS of each native executable.

All executables must print the same checksum or the benchmark fails. Results
are specific to the recorded machine, compiler versions, and flags.

This scalar workload checks loop execution and baseline process memory. It does
**not** test dynamic collection layouts or prove that Swan uses less memory
for data-heavy programs. Swan's current frontend does not yet provide the
dynamic array and ownership features needed to reproduce Goose's memory
benchmarks fairly. Do not claim a memory advantage until those workloads exist
and can be expressed equivalently.

The GitHub Actions workflows run these comparisons on pull requests and can
also be started manually. For device-specific numbers, run the same harness
on the target device with matching native toolchains.
