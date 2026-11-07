# Swan

Swan is a compiled language with a direct x86-64 assembly backend and an optimizing portable backend. The portable backend lowers the typed Swan AST to C and uses Clang/LLVM or the configured C compiler for target-specific machine code.

## Build

    sh build.sh
    ./build/swan examples/hello.swan
    ./build/swan build benchmarks/binary_search.swan -o binary-search

The build currently produces two compiler binaries:

- `build/swan` — full native compiler, with variables, expressions, functions, conditionals, loops, and heap integer arrays.
- `build/swan-c` — standalone C11 compiler subset, retained for the C migration and cross-target assembly work.

The direct assembly backend targets x86-64 Linux (System V ELF). The full-language portable backend supports optimized native builds with `--optimize` and assembly generation for:

- `x86_64` — 64-bit x86
- `aarch64` — 64-bit ARM (modern phones, tablets, Macs, and ARM computers)
- `armv7` — 32-bit ARM
- `riscv64` — 64-bit RISC-V
- `wasm32` — WebAssembly 32-bit

Generate target assembly with `./build/swan program.swan --target aarch64 --emit-asm -o program.s`. Clang supplies the architecture-specific backend. The target aliases above cover common Linux, ARM, RISC-V, and WebAssembly builds; any Clang target triple can also be passed directly, for example `arm64-apple-ios`, `aarch64-apple-darwin`, `aarch64-linux-android24`, `armv7a-linux-androideabi24`, or `x86_64-pc-windows-msvc`. This covers different instruction sets and operating-system ABIs without pretending there is one universal assembly language.

Native optimized builds use `./build/swan program.swan --optimize`; set `CC` to select the native C compiler and `SWAN_CLANG` to select Clang for assembly output. Cross-target executable linking still requires a matching linker and target sysroot; Swan emits target assembly for foreign platforms and leaves final linking to the appropriate platform toolchain.

## Arrays and integer division

`array(size)` allocates a zero-initialized heap array of signed 64-bit integers. Indexing is zero-based. Indexed reads and writes use `values[index]`. Array indexing is currently unchecked, so callers must keep indices within the allocated range.

`/` performs floating-point division. `//` performs signed integer division and returns an integer.

Example:

    fn main() -> int:
        let values = array(8)
        var i = 0
        while i < 8
            values[i] = i * 3
            i = i + 1

        var middle = values[4]
        log middle
        return 0

## Binary-search workload

`benchmarks/binary_search.swan` contains the requested workload: it allocates and initializes a sorted 20,000,000-element array, then performs 20,000,000 deterministic binary searches. The query multiplier is coprime to the array length, so the workload searches every array value once. The expected checksum is:

    199999990000000

Build it with:

    ./build/swan build benchmarks/binary_search.swan -o binary-search

Run it with:

    ./binary-search

## Tests

    sh test.sh

CI verifies heap allocation, indexed loads and stores, integer division, a small binary-search correctness case, and compilation of the full 20M-by-20M workload. It also runs the standalone C compiler's native and cross-target assembly tests.
