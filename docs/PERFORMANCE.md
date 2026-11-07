# Compiler Performance

## Current backend baseline

Swan's initial x86-64 backend emits GNU assembler Intel syntax directly from the semantically checked AST. It has no LLVM, Clang, C source-generation, or external compiler code-generation stage.

The current implementation is a correctness-first baseline. It uses a simple stack-based expression lowering strategy and stack slots for local variables. It is not yet a mature register allocator or optimizing compiler.

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

1. Establish reproducible baselines for frontend, assembly emission, and link stages.
2. Introduce a typed, target-independent Swan low-level IR.
3. Add constant folding, propagation, dead-code elimination, and strength reduction.
4. Implement liveness analysis, linear-scan register allocation, and spill insertion.
5. Add target-specific instruction selection and peephole optimization.
6. Benchmark generated code against equivalent C, C++, Rust, Zig, and Go workloads without mixing compile and runtime measurements.

Correctness and reproducibility take precedence over claimed speedups.
