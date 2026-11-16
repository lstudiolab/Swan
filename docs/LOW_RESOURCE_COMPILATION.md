# Low-Resource and Single-Worker Compilation

**Status:** Current behavior and CLI contract.

## Goal

Swan projects must be buildable on small machines, mobile development environments, CI runners with tight quotas, and memory-constrained servers without requiring a large compiler worker pool.

## Current compiler behavior

The current `swanc` driver is single-worker and sequential:

- It reads the selected Swan source files into one compilation unit.
- Lexing, parsing, import loading, semantic analysis, reachability, IR lowering, optimization, and code generation run sequentially.
- C source assembly and lazy-module builds are launched and waited on one at a time.
- The compiler does not create a thread pool or spawn compiler worker threads.

The external C toolchain and system linker are separate processes and may have their own resource behavior. Swan's single-worker guarantee applies to Swan compiler orchestration, not to every third-party tool or the operating system.

## CLI contract

Use either:

```sh
swanc build src/ --low-resource -o app
swanc build src/ --jobs 1 -o app
```

Both select the single-worker compilation policy. `--low-resource` is the convenient profile name. `--jobs 1` makes the worker limit explicit.

At present, values greater than one are rejected with a clear diagnostic rather than silently ignored. Parallel project compilation is not implemented. This avoids promising a parallel scheduler that does not exist yet.

## Resource behavior and future work

Low-resource compilation should remain deterministic and bounded:

- No compiler worker pool is initialized in single-worker mode.
- No source file is compiled concurrently by the Swan driver.
- Process launches are serialized and reaped before the next tool invocation.
- Future parallel compilation must use a bounded worker pool, never one thread per source file.
- Future `--jobs N` support must cap both active compiler jobs and queued work.
- Memory-heavy phases should process files/modules incrementally where dependency rules permit, rather than retaining duplicate source and token buffers.
- An optional memory budget should fail early with a diagnostic or reduce concurrency; it must not cause uncontrolled swapping or silent data loss.

## Future parallel mode

Parallelism is opt-in. The default remains one worker unless a future release explicitly changes the documented policy. A future implementation may support `--jobs 2`, etc., but only after:

1. Compilation units have independent ownership for AST, token, IR, diagnostics, and temporary outputs.
2. Module dependency graphs are topologically scheduled.
3. Shared caches and interning tables are synchronized or made immutable.
4. Diagnostics and output ordering remain deterministic.
5. Cancellation and worker failure stop dependent work and clean temporary files.
6. Benchmarks show a useful speedup without unacceptable peak-memory growth.

## Acceptance tests

- `--low-resource` compiles a multi-file project successfully.
- `--jobs 1` produces byte-identical output to the default single-worker build where output formats are deterministic.
- `--jobs 0`, missing values, malformed numbers, and values above the currently supported limit produce clear usage errors.
- Stress builds confirm that the driver never has more than one Swan compilation job active.
- Resource benchmarks report wall time, peak RSS, process/thread count, and output checksum.
