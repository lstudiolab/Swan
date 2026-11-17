# Swan native runtime components

This directory contains the first C11/POSIX runtime primitives. They have direct C tests; the JSON handle API is exposed to Swan through `import C` and the x86-64 C ABI.

## Available

- `bytes.h`: overflow-checked reusable byte buffer with geometric growth,
  append (including self-append), resize, clear, and deterministic destruction.
- `json.h`: bounded, zero-copy-token JSON parser and compact/pretty encoder. Its low-level document API borrows input; keep the source buffer alive until document destruction.
- `json_abi.h` / `json_abi.c`: owned-input opaque document handles, root-key integer lookup, key existence, compact/pretty serialization, and parse-error retrieval.
- `json_writer.h` / `json_writer.c`: nested object/array construction, UTF-8 string escaping, integer/boolean/null values, depth limits, and explicit writer state validation.
  The ABI handle owns its input copy and reuses token/output capacity across parses.
  The parser validates UTF-8, JSON escapes, surrogate pairs, number grammar,
  delimiters, nesting depth, input size, and token count.
- `http.h`: bounded HTTP/1.0 and HTTP/1.1 request parser, Content-Length framing, header lookup, and safe response framing. Chunked transfer encoding and socket-server lifecycle are not implemented.
- `http_server.h` / `http_server.c`: bounded HTTP/1.1 JSON echo server using the worker pool. It supports one request per connection, content-length bodies, 503 queue backpressure, and request/byte counters. It does not support chunked transfer encoding, keep-alive, TLS, or custom Swan handlers.
- `thread_pool.h`: bounded POSIX worker pool. Submission is nonblocking;
  a full queue reports backpressure. Shutdown can drain or discard queued work
  and always joins started workers.

## Build and test

```sh
sh tests/runtime-primitives.sh
sh tests/json-runtime.sh
```

The full `sh test.sh` suite includes both test runners.

## Not yet available

The JSON C ABI is a deliberately low-level bridge. String literals are emitted as read-only NUL-terminated bytes and can be passed to imported C pointer parameters on the x86-64 AST compatibility backend. The C importer still represents pointer values as machine-word integers; Swan does not yet have safe pointer types, owned string values, slices, or aggregate ABI lowering.

TLS, chunked transfer decoding, keep-alive/pipelining, generic route handlers, cancellation,
structured task scopes, streaming JSON/JSONL, typed JSON values, and a Swan-level
async scheduler are not implemented. Do not use the C worker pool as evidence
that Swan async semantics exist.


## Minimal Swan HTTP JSON server

A Swan executable can import `runtime/http_server.h`, link the runtime object,
and run the built-in JSON echo server:

```swan
import C "runtime/http_server.h"

func main() -> int:
    let server = swan_http_json_server_create(8080, 8, 256)
    if server == 0:
        return 1
    return swan_http_json_server_run(server)
```

The server binds all interfaces. It accepts one request per connection, parses
JSON request bodies, and returns compact JSON. The blocking run call must be
stopped from another thread before destroying the handle. It is a runtime
foundation, not yet a full web framework or production TLS endpoint.
