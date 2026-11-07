# Swan

Swan is a compiled language. Its native compiler emits x86-64 System V assembly directly and links a native executable; it does not transpile programs to C.

## Build

    sh build.sh
    ./build/swan examples/hello.swan
    ./build/swan build benchmarks/binary_search.swan -o binary-search

The build currently produces two compiler binaries:

- `build/swan` — full native compiler, with variables, expressions, functions, conditionals, loops, and heap integer arrays.
- `build/swan-c` — standalone C11 compiler subset, retained for the C migration and cross-target assembly work.

The full native assembly backend currently targets x86-64 Linux (System V ELF). The standalone C subset can emit x86-64, AArch64, and ARMv7 assembly.

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
