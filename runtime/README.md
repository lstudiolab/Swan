<p align="center"><img src="../assets/brew-logo.png" alt="Brew" width="180"></p>

# Brew native runtime components

This directory contains the first C11/POSIX runtime primitives. They have direct C tests; the JSON handle API is exposed to Brew through `import C` and the x86-64 C ABI.

## Available

- `arena.h`: fixed-capacity, caller-owned bump arena with aligned allocation,
  overflow-safe bounds checks, zeroed allocation, marks, rewind, and reset.
  It performs no heap allocation and never silently grows its backing storage.
- `bytes.h`: overflow-checked reusable byte buffer with geometric growth,
  append (including self-append), resize, clear, and deterministic destruction.
  Caller-owned fixed storage is also supported; it never falls back to heap growth.
- `json.h`: bounded JSON parser and compact/pretty encoder. The low-level
  document API borrows its input buffer and supports caller-owned token storage
  for allocation-free parsing; fixed buffers fail explicitly when exhausted.
- `json_abi.h` / `json_abi.c`: owned-input opaque document handles, root-key
  integer lookup, key existence, compact/pretty serialization, and parse errors.
  The handle reuses token and output capacity across repeated parses.
- `json_writer.h` / `json_writer.c`: bounded-depth incremental object/array
  writer for escaped UTF-8 strings, i64, booleans, and null. It enforces one
  root value and correct object key/value order; float and typed-struct output
  are not exposed yet.
- `http.h`: bounded HTTP/1.0 and HTTP/1.1 request parser, Content-Length
  framing, case-insensitive header lookup, and safe response framing.
- `http_server.h` / `http_server.c`: bounded HTTP/1.1 JSON echo/health server
  using the worker pool, with body limits, socket timeouts, 503 queue
  backpressure, and request/byte counters. One request per connection.
- `thread_pool.h`: bounded POSIX worker pool. Submission is nonblocking;
  a full queue reports backpressure. Shutdown can drain or discard queued work
  and always joins started workers.

## Build and test

```sh
sh tests/runtime-primitives.sh
sh tests/json-runtime.sh
sh tests/json-writer.sh
sh tests/http-runtime.sh
sh tests/http-server.sh
```

The full `sh test.sh` suite includes all runtime and Brew C-ABI integration tests.

## Not yet available

The JSON C ABI is a deliberately low-level bridge. String literals are emitted as read-only NUL-terminated bytes and can be passed to imported C pointer parameters on the x86-64 AST compatibility backend. The C importer still represents pointer values as machine-word integers; Brew does not yet have safe pointer types, owned string values, slices, or aggregate ABI lowering.

TLS, chunked transfer decoding, keep-alive/pipelining, generic route handlers, cancellation,
structured task scopes, streaming JSON/JSONL, typed JSON values, and a Brew-level
async scheduler are not implemented. Do not use the C worker pool as evidence
that Brew async semantics exist.


## Minimal Brew HTTP JSON server

A Brew executable can import `runtime/http_server.h`, link the runtime object,
and run the built-in JSON echo server:

```brew
import C "runtime/http_server.h"

func main() -> int:
    let server = brew_http_json_server_create(8080, 8, 256)
    if server == 0:
        return 1
    return brew_http_json_server_run(server)
```

Build the combined runtime object and link it with the Brew program:

```sh
cc -std=c11 -O2 -pthread -c runtime/http_json.c -o http_json.o
brew build server.brew --link http_json.o -o server
```

The server binds all interfaces. It accepts one request per connection, parses
JSON request bodies, and returns compact JSON. The blocking run call must be
stopped from another thread before destroying the handle. It is a runtime
foundation, not yet a full web framework or production TLS endpoint.
