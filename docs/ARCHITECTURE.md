# Compiler Architecture

Swan owns its frontend and machine-code generation. There is no C transpilation stage and no LLVM or Clang backend.

## Implementation model

The compiler is C11. `main.c` is the only compiled translation unit; all compiler subsystems are headers under `compiler/include/swan/`.

## Active pipeline

```text
.swan source
  -> lexer and indentation tokens
  -> parser and AST
  -> standard-library import resolution
  -> semantic name/scope validation
  -> direct AST-to-x86-64 lowering
  -> GNU assembler Intel syntax
  -> system assembler and linker
  -> executable
```

The assembler and linker consume only Swan-generated assembly. They do not compile Swan source or select Swan instructions.

## Implemented backend components

- x86-64 System V function entry, stack-frame layout, and call alignment.
- Up to six integer/pointer arguments through the standard register argument sequence.
- Callee-saved and caller-saved local register allocation, with stack spills.
- Direct local arithmetic and comparison lowering.
- Constant folding, short-circuit logical lowering, power-of-two division, small single-return inlining, and loop strength reduction.
- Bounds-checked and unchecked contiguous `u32` arrays.
- SAMC compiler-tracked array ownership, automatic function-exit cleanup, manual ownership declarations, and explicit `free` lowering without runtime reference counters.
- Loop break/continue labels and early-exit-aware loop lowering.
- Error-checked assembly-buffer growth.

The current backend lowers directly from the AST. SAMC conservatively releases non-escaping local array allocations at function exit and on early returns. Arrays that escape through aliases, calls, or returns are not implicitly released. SAMC does not yet have interprocedural ownership analysis or per-block last-use cleanup. The backend does not yet have a typed, target-independent IR, global liveness analysis, or a mature optimization pipeline.

## Standard library

Top-level `import module` declarations are resolved from the configured standard-library directory. Module source buffers remain alive until semantic analysis and code generation finish. Repeated imports are loaded once, and nested imports are resolved before semantic checking.

## Backend architecture target

Each additional target must own:
1. Target data layout and ABI lowering.
2. Instruction selection from a typed low-level IR.
3. Register allocation and spill handling.
4. Calling convention and stack-frame layout.
5. Assembly emission and relocation/symbol conventions.
6. Target-specific optimization and instruction scheduling.

Planned Swan-owned backends: AArch64, ARMv7/Thumb, RISC-V 64, and WebAssembly.

## Engineering rules

1. Never transpile Swan source into C.
2. Never delegate Swan code generation to LLVM or Clang.
3. System assemblers/linkers may consume Swan-emitted assembly.
4. Never advertise a target until its own backend passes ABI and execution tests.
5. Every optimization requires correctness tests and reproducible performance measurements.
