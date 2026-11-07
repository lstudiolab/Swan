# Swan

Swan is a compiled language. The active compiler is being migrated to standalone C11 and emits native assembly directly; it does not generate C as an intermediate representation.

## Build

    sh build.sh
    ./build/swan examples/hello.swan --target native
    ./build/swan examples/hello.swan --target native --emit-asm -o hello.s


Supported assembly targets:

- `x86_64` — GNU assembler, System V ELF.
- `aarch64` — GNU assembler, AArch64 ELF.
- `armv7` — GNU assembler, ARMv7 ELF.

`native` selects the architecture on which the C compiler was built. The selected target controls assembly syntax and instruction selection. Emitting a non-host target's assembly does not require a cross toolchain; assembling/linking it does.

## Current C migration status

The C11 compiler is active in `compiler-c/swan.c`. This first executable slice supports `fn main()`, string `log`, integer-literal `return`, and `pass`, and emits target-specific assembly for all three targets. Native executable creation uses `CC` (default `cc`).

The previous C++ compiler and its frontend remain in the repository as migration reference material, but are no longer part of `build.sh` or CI. The language frontend and backend are not yet fully ported: variables, expressions, functions, control flow, semantic analysis, and the standard library are pending. Unsupported syntax is rejected rather than silently ignored.

## Tests

    sh test.sh

The active tests build the C compiler, run a native hello program, verify emitted assembly for x86-64/AArch64/ARMv7, and check invalid-target rejection.
