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

On the recorded Ubuntu 26.04 / Clang 21.1.8 runner, backend compilation
ranged from about 47–53 ms for `-O0`/`-O1` across recent runs. `-O0` was
occasionally a few milliseconds faster, but its runtime varied sharply: one
run was 151 ms and another was 253 ms, while `-O1` remained around 168 ms.
Swan therefore keeps `-O1` as the default to preserve predictable runtime.
Set `SWAN_OPT_LEVEL=0` when compile time matters more than runtime. Results are
workload- and runner-specific.

## Nine-language comparison

Run after installing the C/C++ toolchain, Rust, Goose, Swift, Zig 0.13, Go,
Python 3, and GNU time:

```sh
sh build.sh
python3 benchmarks/compare_languages.py
```

The same 50-million-iteration signed 32-bit recurrence is implemented in
Swan, Rust, C++, Goose, C, Swift, Zig, Go, and Python. All implementations
must print the same checksum or the benchmark fails. Native implementations
are built with release-oriented optimization settings: Swan/Goose/C/C++ use
Clang `-O2`, Rust uses `-C opt-level=3`, Swift uses `-O`, Zig uses
`ReleaseFast`, and Go uses its standard optimized build. Python is run
interpreted and reports compile time as N/A.

Compile time is a clean one-shot measurement of the full frontend/backend
pipeline. Runtime is the median of five runs after one warm-up. Peak runtime
RSS is measured with GNU time. Compiler RSS is omitted because some compiler
drivers spawn child processes and parent-only RSS would not be comparable.
These results are specific to the machine, compiler versions, and flags;
they are not a universal language ranking.

### Recorded nine-language result

Ubuntu 24.04.5 x86_64, Clang/Clang++ 17.0.0, Rust 1.99.1, Go 1.27.3,
Swift 6.1, Zig 0.13.0, Goose v2026.4. The benchmark used 50 million
iterations of the same signed 32-bit recurrence and checksum `966250007`.

| Language | Compile pipeline (s) | Runtime (s) | Runtime peak RSS (MiB) |
|---|---:|---:|---:|
| Swan | 0.0520 | 0.1260 | 1.59 |
| Rust | 0.7276 | 0.1284 | 2.20 |
| Goose | 0.6690 | 0.1574 | 2.59 |
| C | 0.0322 | 0.1269 | 1.59 |
| C++ | 0.2512 | 0.1291 | 3.60 |
| Swift | 0.2458 | 0.1275 | 9.07 |
| Zig | 5.7527 | 0.1609 | 0.26 |
| Go | 2.9109 | 0.1314 | 1.93 |
| Python (interpreted) | N/A | 3.0514 | 10.17 |

On this single workload, Swan's runtime was within measurement noise of C,
C++, Rust, Swift, and Go. Goose and Zig were somewhat slower on this loop;
Python took about 24.2 times as long as Swan. Swan's full compile pipeline
was 52 ms, versus 32 ms for direct C compilation. Zig and Go had the longest
one-shot compile times in this run. These are observations from one runner,
not general language rankings or guarantees.

All nine implementations produced the same checksum. Native compiler flags
were Swan/Goose/C/C++ Clang `-O2`, Rust `opt-level=3`, Swift `-O`,
Zig `ReleaseFast`, and Go's default optimized build. Runtime is the median
of five trials after one warm-up; compile time is a single clean pipeline
measurement. RSS is the executable process peak, measured by GNU time.
Compiler RSS is omitted because some compiler drivers spawn child processes.

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
