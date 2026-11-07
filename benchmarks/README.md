# Swan performance benchmarks

## Cross-language baseline

Run after building Swan and installing Clang, Rust, Goose, and GNU time:

```sh
sh build.sh
python3 benchmarks/compare_languages.py
```

The harness runs equivalent 50-million-iteration scalar loops in Swan, Rust,
and Goose. Swan and Goose both emit C, which is compiled with the same Clang
version and optimization flags. Rust uses rustc with optimization level 3 and
the native CPU target. It reports end-to-end compile-pipeline wall time, peak
RSS for the largest individual compiler/backend process, median native
execution time after one warm-up, and peak RSS of each native executable.

All executables must print the same checksum or the benchmark fails. Results
are specific to the recorded machine, compiler versions, and flags.

This scalar workload checks loop execution and baseline process memory. It does
**not** test dynamic collection layouts or prove that Swan uses less memory
for data-heavy programs. Swan's current frontend does not yet provide the
dynamic array and ownership features needed to reproduce Goose's memory
benchmarks fairly. Do not claim a memory advantage until those workloads exist
and can be expressed equivalently.

The GitHub Actions workflow runs this comparison on pull requests and can also
be started manually. For device-specific numbers, run the same harness on the
target device with matching native toolchains.
