# Swan compiler benchmarks

Run from the repository root:

```sh
python3 benchmarks/compile.py
```

The harness measures end-to-end compilation of a deterministic generated source file
containing 2,500 declarations. It reports seven wall-clock samples and their median.
Use the same machine, compiler build, and fixture before and after a change. This
benchmark is intended for regression tracking; it does not isolate individual compiler
phases or measure peak memory.
