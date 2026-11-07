# Swan

Swan is a compiled language with its own C11 compiler implementation and Swan-owned assembly code generation. It does not transpile Swan source to C and does not use LLVM or Clang.

## Compiler architecture

The compiler is implemented in C11. \`main.c\` is the only compiled source file. All compiler subsystems are header-only C modules under \`compiler/include/swan/\`.

~~~text
.swan source
  -> C header-only lexer
  -> C header-only parser
  -> C semantic analysis
  -> Swan AST
  -> Swan-owned x86-64 assembly generator
  -> GNU assembler / system linker
  -> executable
~~~

The system assembler and linker only consume assembly emitted by Swan. They do not compile Swan source or select Swan instructions.

## Build and run

Requirements: a C11 compiler and a system assembler/linker (\`cc\` on Linux).

~~~sh
sh build.sh
./build/swan examples/hello.swan
~~~

Emit assembly without assembling or running:

~~~sh
./build/swan examples/hello.swan --emit-asm -o hello.s
~~~

## Arrays

The x86-64 backend supports contiguous 32-bit unsigned integer arrays:

- `array_u32(length)` allocates a zero-initialized array with bounds-checked indexing.
- `array_u32_unchecked(length)` allocates the same contiguous storage without per-access bounds checks for performance-critical code.
- Read an element with `values[index]` and write with `values[index] = value`.
- Array allocation uses the system C allocator; the operating system reclaims the memory when the process exits.

The one-billion-element native sorting workload is in `benchmarks/sort_1b.swan`. It uses approximately 4 GB for the array and performs generation, in-place heapsort, and an orderedness verification pass. It is intentionally not run in ordinary CI because it requires substantial memory and execution time; CI compiles and links it and runs a small heapsort correctness test.

## Translation-unit policy

- Exactly one compiled source file: \`main.c\`.
- No C++ source files or C++ headers remain.
- Compiler modules are C headers and are included by \`main.c\`.
- The build uses \`cc -std=c11\`; it does not require a C++ compiler.

## Backend status

| Target | C backend status |
|---|---|
| x86-64 System V (Linux) | Initial implementation; tested |
| AArch64 / ARM64 | Not ported yet |
| ARMv7 / Thumb | Not ported yet |
| RISC-V | Not ported yet |
| WebAssembly | Not ported yet |

The current C backend is an initial subset. It supports integer and boolean expressions, local variables, function calls, \`if\`/\`else\`, \`while\`, and console logging. Arrays, floating point, structs, \`break\`/\`continue\`, full type checking, and other targets still need to be ported.

## Tests

~~~sh
sh test.sh
~~~

CI verifies the one-file C11 build, standalone C headers, semantic checks, emitted x86-64 assembly, and native execution.
