# Compiler Performance

## Current backend baseline

Swan's initial x86-64 backend emits GNU assembler Intel syntax directly from the semantically checked AST. It has no LLVM, Clang, C source-generation, or external compiler code-generation stage.

The current implementation is a correctness-first baseline. It uses a simple stack-based expression lowering strategy and stack slots for local variables. It is not yet a mature register allocator or optimizing compiler.

## Implemented optimizations

The current compiler hot paths use:
- **Arena-backed AST storage:** expression nodes, statement nodes, and parsed block vectors are allocated from 64 KiB chunks and released together.
- **Compact tokens:** token records store text pointers into a shared token-text buffer instead of embedding a 512-byte character array in every token.
- **Indexed semantic lookup:** function declarations and scoped local symbols use hash buckets rather than repeated linear scans. Scope rollback restores prior bucket heads without copying entire symbol tables.
- **Indexed backend locals:** compact local records and hash lookup replace repeated reverse scans through large fixed-size local arrays.
- **Cached AST call metadata:** expression trees carry call-presence metadata so call analysis does not repeatedly walk subtrees.
- **Low-overhead assembly emission:** literal instruction strings bypass the formatting library; formatted output uses a single formatting pass unless the output buffer must grow.
- **Reduced runtime instruction count:** local-to-local arithmetic and comparisons avoid push/pop stack temporaries. Self-updates such as `sum = sum + value`, `value = value - 1`, and `value = value * factor` now update the destination register or stack slot directly instead of calculating into `rax` and copying the result back. Existing constant folding, small-function inlining, register locals, leaf-frame elimination, parity checks, and loop strength reduction remain enabled.

These changes remove known allocation, lookup, formatting, and instruction-generation overhead. Their speed impact is **not quantified** until a separately authorized benchmark is run.

## Required measurements

Track separately:
- Swan frontend and semantic-analysis time.
- Swan assembly-generation time.
- System assembler/linker time.
- End-to-end compile time.
- Generated assembly size and object size.
- Runtime and peak memory.

Every result must record compiler revision, CPU, OS, assembler/linker version, optimization settings, workload, and output checksum.

## Optimization roadmap

1. Replace fixed-size token text storage with compact source spans and a dedicated decoded-string pool.
2. Introduce a typed, target-independent Swan low-level IR.
3. Add control-flow-aware constant propagation and dead-code elimination.
4. Implement liveness analysis, linear-scan register allocation, and spill insertion.
5. Add target-specific instruction selection and peephole optimization.
6. Benchmark generated code against equivalent C, C++, Rust, Zig, and Go workloads without mixing compile and runtime measurements.

Correctness and reproducibility take precedence over claimed speedups.
