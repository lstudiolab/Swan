# Swan Data-Center Runtime and Backend Contract

**Status:** Runtime capability snapshot and implementation contract. The table distinguishes working components from their explicit boundaries.

## Current implementation status

The following runtime pieces are now present and covered by the repository test
suite:

| Component | Implemented surface | Boundary |
|---|---|---|
| Byte buffer | Overflow-checked C buffer with amortized growth and self-append | C API only; not a Swan slice/string type |\n| Fixed-capacity arena | Caller-owned bump allocator with aligned allocation, zeroing, marks, rewind, and reset; no heap growth | Runtime primitive only; compiler scratch allocation and automatic object ownership are not yet routed through it |
| JSON parser | Token-tree DOM over borrowed input; strict UTF-8, escapes, surrogate pairs, number grammar, depth/input/token limits | C API; i64 and finite f64 accessors; caller-controlled token preallocation, caller-owned fixed-capacity storage for zero-heap parsing, and reuse; original token spans remain available for exact-decimal handling |
| JSON output | Compact/pretty serialization plus a nested object/array writer with UTF-8 escaping | Writer supports strings, i64, booleans, null, and containers; floating-point writer values and a typed Swan DOM remain open |
| Swan JSON bridge | direct `import C` root-key lookup, integer access, key existence, encode, error and close | x86-64 C ABI with opaque integer handles; no safe pointer/string types |
| JSON writer | Swan-importable C ABI for nested objects/arrays, escaped UTF-8 strings, i64, booleans, and null | C ABI only; one root value, max depth 128; no float writer or typed DTO generation |
| Worker pool | Bounded POSIX queue, nonblocking submit/backpressure, drain or discard shutdown | C/POSIX primitive only; no Swan task scheduler |
| HTTP JSON server | Bounded POSIX HTTP/1.1 JSON echo/health server, worker-pool dispatch, queue backpressure, body limits, socket timeouts, counters | JSON-only fixed handler; one request per connection; no TLS, chunked transfer, keep-alive, or user-defined Swan route callbacks |
| HTTP/1 parser | Bounded HTTP/1.0/1.1 request-line/header parser, Content-Length framing, case-insensitive header lookup, safe response framing | C API only; chunked transfer and keep-alive are not supported |
| String FFI | NUL-terminated literals passed to imported C pointer parameters | x86-64 AST compatibility backend only |

TLS, chunked transfer, generic route dispatch, SQL drivers, streaming JSONL, generic dynamic collections,
aggregate ABI/lifetimes, and language-level async remain unimplemented. The
benchmark work must report the exact runtime path used by each language; a C
runtime call from Swan must not be described as a pure Swan JSON implementation.

## Goal

Make Swan a native, ahead-of-time compiled language suitable for long-running backend services, database-heavy applications, JSON APIs, and data-center workloads. The target is Go-class practicality and throughput, measured on equivalent workloads. No performance ranking is claimed until repeatable benchmarks exist.

Preserve the production compiler model:

- C11 compiler frontend and Swan-owned typed IR/SVM pipeline.
- Native code generation; no interpreter, bytecode VM, or mandatory managed runtime.
- Whole-project compilation and the existing C interoperability path.
- Explicit, phase-gated diagnostics. Unsupported source constructs must fail compilation.

Do not replace the compiler architecture to obtain these capabilities.

## Non-negotiable implementation rules

1. A standard-library API is implemented only when its parser, semantic model, lowering, runtime, supported backend, diagnostics, and tests work together.
2. Never add placeholder APIs that return zero, empty values, or success without doing the operation.
3. Keep platform-specific code behind a stable C ABI/runtime boundary. Prefer portable C11 for shared runtime code and small OS-specific adapters where required.
4. Make ownership and lifetime rules explicit before exposing buffers, strings, rows, or asynchronous values.
5. Check every allocation-size calculation for overflow. Propagate allocation and I/O failures.
6. Keep blocking database and filesystem operations out of scheduler threads. Cancellation and shutdown must release resources deterministically.
7. Benchmark release builds on pinned workloads; report throughput, latency percentiles, allocations/bytes, peak resident memory, and checksums.
8. Keep the existing `!` standard-library namespace and `!printin >> value` output intrinsic. Do not add competing print/log APIs.

## Runtime architecture

The runtime should be small and separable:

- **Core:** fixed-width scalar types, checked arithmetic helpers, byte spans, strings, and error values.
- **Memory:** allocator interface, arenas for request-scoped work, reusable buffers, and explicit ownership/lifetime rules. Do not commit to a tracing garbage collector before measuring its pause and throughput costs.
- **Collections:** dynamically sized typed arrays, slices, hash maps, and byte buffers. Capacity growth must be amortized and overflow-safe.
- **OS:** files, directories, environment, clocks, timers, processes, signals, and graceful shutdown.
- **Networking:** TCP/UDP, HTTP client/server, TLS integration, and WebSockets. Use nonblocking I/O and backpressure for high-concurrency servers.
- **Concurrency:** lightweight tasks, bounded worker pools, channels/queues, mutexes, read/write locks, atomics, cancellation, and structured shutdown. Define scheduler fairness and blocking-call behavior.
- **Serialization:** JSON first, then JSON Lines and other formats. Share byte-buffer and I/O primitives rather than building separate allocation-heavy paths.
- **Database:** common driver contract, connection pools, prepared statements, transactions, cancellation, and dialect-specific drivers.

The runtime must not require every application to start a scheduler or initialize a database subsystem. Each subsystem should initialize lazily or through explicit application setup.

## JSON design

JSON is a core backend workload. Implement both a DOM-style API for convenient access and a streaming API for large payloads.

### Required behavior

- Parse objects, arrays, strings, numbers, booleans, and null with strict UTF-8 and escape validation.
- Correctly handle surrogate pairs and reject malformed Unicode escapes.
- Provide checked signed-integer and finite binary64 accessors; preserve original numeric token spans for consumers requiring exact decimal precision. Binary64 conversion is not an exact-decimal representation.
- Encode strings with correct escaping; never emit invalid JSON.
- Provide compact and pretty output.
- Support JSON Lines without loading the entire file.
- Support incremental parsing from a byte buffer or reader.
- Provide typed decoding/encoding once Swan's struct/type metadata is implemented.
- Enforce configurable nesting, input-size, and token limits to defend services against hostile payloads.
- Return structured errors with byte offset, line, column, and reason.
- Avoid copying source string slices where their lifetime can safely be tied to the input buffer. Provide an owned form when data must outlive the input.
- Use a growable writer with geometric capacity growth and a caller-provided buffer option.
- Write files through a temporary file plus atomic rename where the platform supports it; never leave a partially written target file on serialization failure.

### Performance requirements

- No per-character heap allocation.
- No allocation for parser bookkeeping proportional to input bytes when token count is sufficient.
- Reuse caller-provided buffers for repeated request/response workloads. Allow token capacity to be reserved before parsing when request-size profiles are known, avoiding geometric growth reallocations. Callers may instead attach fixed-capacity token storage; parsing then performs no token-heap allocation and fails explicitly when the supplied capacity is exhausted.
- Benchmark parse-only, encode-only, parse+encode, nested objects, large arrays, escaped strings, and streaming JSONL separately.
- Validate output by reparsing and comparing canonical test data; benchmark checksums must match.

### Intended source-level shape

The following is a design sketch, not currently compilable API:

```swan
import std_json

func main() -> int:
    let document = !json_parse("{\"service\":\"swan\",\"workers\":128}")
    let workers = !json_get_int(document, "workers")
    let output = !json_encode(document)
    !printin >> workers
    !printin >> output
    return 0
```

The final API should use typed structures and direct property access after struct/member semantics exist. Avoid forcing applications to manually traverse untyped key/value trees for common request DTOs.

## SQL and database design

Provide a shared database contract with separately maintained drivers. Initial driver order:

1. SQLite for deterministic local tests and embedded workloads.
2. PostgreSQL for production services and connection-pool validation.
3. MySQL after the shared driver contract is stable.

### Required behavior

- Open/close a database and report connection errors.
- Configurable connection pools with maximum open/idle connections, lifetime, idle timeout, and health checks.
- Prepared statements and parameter binding; never interpolate untrusted values into SQL.
- Typed row decoding with explicit null handling.
- Transactions with commit, rollback, and rollback-on-scope-exit semantics once deterministic cleanup exists.
- Context-aware query cancellation and deadlines.
- Query/exec distinction, affected-row counts, and last-insert identifiers where supported.
- Bounded pool queues and backpressure rather than unbounded connection creation.
- Metrics for wait time, active/idle connections, query duration, and failures.
- Migrations with ordered versions, checksums, and transactional application where supported.
- Clear driver capability reporting; do not pretend all SQL dialects behave identically.

### Intended source-level shape

The following is a design sketch, not currently compilable API:

```swan
import std_sql

func main() -> int:
    let db = !sql_open("postgres", DATABASE_URL)
    let rows = !sql_query(db, "SELECT id, name FROM users WHERE active = ?", true)

    for row in rows:
        !printin >> row.name

    !sql_close(db)
    return 0
```

The production API should replace untyped handles and placeholder row values with typed statements, row decoders, and scoped transactions when Swan's type, aggregate, and lifetime systems can enforce those contracts. Driver handles must not be exposed as forgeable raw pointers.

## Compiler prerequisites

The current compiler's typed IR has scalar integer, floating-point, and pointer categories, plus calls and basic memory operations. That is not sufficient by itself for safe high-level JSON and SQL APIs. Implement these prerequisites in dependency order:

1. **Strings and byte spans:** explicit length, capacity, encoding contract, immutable/mutable distinction, and lifetime rules.
2. **Aggregate layout:** complete struct declarations, field offsets/alignment, aggregate parameters/returns, and target ABI lowering.
3. **Dynamic collections:** typed slices/arrays and maps with checked indexing and well-defined ownership.
4. **Errors and cleanup:** structured error propagation and guaranteed cleanup on every return path.
5. **C ABI/runtime calls:** stable declarations for runtime entry points, correct pointer and string passing, and link-time inclusion of runtime objects.
6. **Concurrency and I/O:** scheduler, blocking-I/O integration, cancellation, and shutdown semantics.
7. **JSON and SQL APIs:** build only on the preceding stable primitives.

The compiler must reject unsupported aggregate, pointer, or async operations on targets that cannot lower them. Do not let a feature's presence in the AST or IR enum imply backend support.

## Delivery milestones and acceptance gates

### Milestone A — data representation

- Complete strings/bytes, struct layout, typed dynamic arrays, maps, and deterministic cleanup.
- Unit tests cover empty values, boundary sizes, allocation failure, overflow, nested aggregates, and cross-function ownership.
- Typed IR verifier and x86-64 backend agree on layout and calling convention.

### Milestone B — JSON

- DOM parser/encoder, streaming parser/writer, JSONL, and atomic file output.
- Fuzz malformed inputs and run sanitizer builds.
- Golden tests cover Unicode, numeric precision, escapes, deep nesting, truncation, and large inputs.
- Benchmarks report throughput, p50/p95/p99 latency, allocations, peak memory, and verified checksums.

### Milestone C — SQL

- SQLite driver first, then PostgreSQL.
- Tests cover pool limits, prepared statements, nulls, transactions, rollback, cancellation, concurrent callers, reconnects, and shutdown.
- Integration tests run against real database engines; mocks alone are insufficient.

### Milestone D — service runtime

- HTTP server/client, TCP, TLS, WebSockets, bounded concurrency, backpressure, graceful shutdown, and observability hooks.
- Load tests report requests/second, p50/p95/p99 latency, error rate, CPU, memory, and open connections.
- Test slow clients, abrupt disconnects, overload, cancellation, and partial network failures.

### Milestone E — portability and release quality

- Bring the runtime to each advertised native target, or clearly mark unsupported modules per target.
- Cross-compile and run target tests where execution infrastructure exists.
- Publish reproducible benchmark commands, machine details, compiler flags, and raw results.
- Do not advertise "Go-level" or "faster than Go" until equivalent benchmark suites establish the claim.

## Benchmark policy

Every performance-sensitive subsystem gets an isolated microbenchmark and at least one end-to-end workload. Compare Swan with Go using equivalent algorithms, data, concurrency, compiler optimization, and machine settings. Include warmup and repeated runs; use medians and tail latency. Keep correctness checks enabled outside timed regions and verify identical output checksums.

A faster parser is not enough to make a backend language. End-to-end service throughput, tail latency under contention, memory stability, cancellation behavior, and operational reliability are release gates.
