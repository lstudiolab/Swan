<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Brew Standard Library

The standard library is implemented in Brew source under `std/`. Import it explicitly, then call functions by their ordinary names:

```brew
import std
import std_math

func main() -> int:
    print(abs(-42))
    print(gcd(84, 30))
    return 0
```

`print(expression)` is Brew's standard output call. Other functions are ordinary Brew functions loaded from the imported module source.

## Implemented Brew modules

### `std` — core integer helpers

Source: `std/std.brew`

Integer helpers that can overflow return `0` as an overflow sentinel. This includes
`abs(INT64_MIN)`, out-of-range `square`/`cube`, overflowing `power`,
`factorial`, and `sum_to`. Because zero can also be a valid result, callers that
need to distinguish overflow from a mathematical zero must validate inputs first.
`average` avoids overflowing the intermediate sum and truncates toward zero.

- `abs`, `min`, `max`, `clamp`, `sign`
- `square`, `cube`, `power`, `factorial`, `sum_to`
- `is_even`, `is_odd`, `in_range`, `average`
- `print(value)` output call

### `foundation` — compatibility integer helpers

Source: `std/foundation.brew`

This module preserves the earlier `*_int` helper names for existing programs.
Its arithmetic has the same signed 64-bit overflow sentinel behavior as `std`.
New programs should generally prefer `std`; Foundation is retained for import
compatibility and is not a separate platform API.

### `std_math` — integer mathematics

Source: `std/std_math.brew`

- Number theory: `gcd`, `lcm`, `is_prime`, `is_composite`
- Division and remainder: `floor_div`, `ceil_div`, `euclidean_mod`
- Integer roots and sequences: `integer_sqrt`, `fibonacci`, `triangular`
- Integer utilities: `distance_int`, `is_power_of_two`, `next_power_of_two`

Import `std_math` to use these functions. The module uses only Brew integer expressions, conditions, loops, local bindings, and direct function calls. Invalid divisors/moduli and unrepresentable absolute results or overflows use `0` as a sentinel; callers must validate inputs when zero is a meaningful result.

### `std_collections` — u32 arrays and sequence algorithms

Source: `std/std_collections.brew`

- Queries: `array_sum`, `array_min`, `array_max`, `array_contains`, `array_index_of`, `array_count`, `array_average`, `array_is_sorted`
- Mutation: `array_fill`, `array_copy`, `array_reverse`
- Search: `array_linear_search`, `array_binary_search`
- Sorting: `array_bubble_sort`, `array_selection_sort`, `array_insertion_sort`, `array_heap_sort`

The current collection API operates on Brew's implemented `u32_array` type. Every operation takes an explicit nonnegative logical length, which must not exceed the allocated array length; violating the capacity precondition triggers the compiler's checked-index trap. Binary search requires sorted input. Empty min/max/average results and invalid fill lengths/values use `0` as a sentinel. `array_sum` also returns `0` if its signed result would overflow. It does not pretend to provide generic `Array<T>`, dynamic capacity, slices, or collection protocols.

### Native JSON runtime (C ABI)

The JSON runtime is implemented in `runtime/json.h`, `runtime/json_abi.c`, and
`runtime/json_writer.c`. Brew programs can import the ABI headers with
`import C` and link the runtime object. It provides reusable document handles,
parsing, root-key integer lookup, key existence, compact/pretty serialization,
parse-error retrieval, and a nested object/array writer with UTF-8 escaping.

This is a low-level x86-64 C-ABI surface. Opaque document and returned string
pointers are represented as integer-register values because Brew does not yet
have safe pointer or general string types. It is not yet a typed generic DOM, streaming reader, floating-point writer, or JSON Lines API.

### Native HTTP JSON service (C ABI)

`runtime/http_server.h` exposes a bounded HTTP/1.1 JSON echo/health server.
It uses the POSIX worker pool, enforces request body limits and socket timeouts,
returns 503 when its bounded queue is full, and exposes request/byte counters.
The current service has fixed JSON echo behavior and closes each connection;
TLS, chunked transfer, keep-alive, and user-defined Brew route callbacks are not
implemented.

## Backend runtime development order

The data-center runtime is being developed in dependency order: strings/bytes and aggregate layout; typed dynamic collections and deterministic cleanup; runtime ABI and I/O; JSON parsing/encoding and streaming; SQL drivers and connection pools; then HTTP services and scalable concurrency. See [the data-center runtime contract](../docs/data_center_runtime.md) for API requirements, security constraints, benchmarks, and acceptance gates. These modules must not be represented by stub functions or advertised as available before their complete implementations and tests exist.

## Compatibility and correctness boundaries

The current language/backend subset is not yet sufficient to implement all Swift and Goose standard-library surfaces in Brew source. The following capabilities must not be advertised as implemented until Brew has the required language/runtime support and end-to-end tests:

- Generic typed collections, dictionaries, sets, iterators, slices, and collection protocols.
- General-purpose strings, Unicode scalar/grapheme processing, formatting, and parsing.
- Floating-point math, transcendental functions, and IEEE-754 classification.
- File and directory I/O, processes, environment access, general networking and sockets. The limited HTTP/1.1 JSON echo server is a C ABI service, not a general Brew networking API.
- CSV, Base64, regular expressions, general-purpose string operations, and a full typed JSON DOM/streaming API.
- Safe pointers, allocators, ownership/borrowing, and memory layout APIs.
- Tasks, async sequences, actors, cancellation, and a language-level scheduler. The POSIX C worker-pool primitive is not Brew async.
- Reflection, key paths, runtime metadata, and serialization protocols.
- Graphics, audio, physics, and UI frameworks.

These are part of the implementation target, not fake stubs. Each module will be added only when its Brew implementation can compile and its behavior can be tested on the supported targets. Swift Foundation and Goose's platform/game modules are not equivalent to Swift's core `Swift` module.

## Build and test

```sh
sh build.sh
sh tests/std-library.sh
sh tests/std-math.sh
```

The complete suite runs both standard-library tests through `sh test.sh`. Native standard-library import and linking currently use the x86-64 System V compatibility backend. Do not assume standard-library support on cross-target backends until target-specific import, linking, and runtime tests pass.
