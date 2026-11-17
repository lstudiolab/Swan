# Swan native runtime components

This directory contains the first C11/POSIX runtime primitives. They have direct C tests; the JSON handle API is also exposed to Swan through the x86-64 C ABI.

## Available

- `bytes.h`: overflow-checked reusable byte buffer with geometric growth,
  append (including self-append), resize, clear, and deterministic destruction.
- `json.h`: bounded, zero-copy-token JSON parser and compact/pretty encoder.
- `json_abi.h` / `json_abi.c`: owned-input opaque document handles, root-key integer lookup, key existence, compact/pretty serialization, and parse-error retrieval.
  The document borrows its input buffer; keep the input alive until the document
  is destroyed. It validates UTF-8, JSON escapes, surrogate pairs, number grammar,
  delimiters, nesting depth, input size, and token count.
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

The `std_json` source module is a deliberately low-level bridge. String literals are emitted as read-only NUL-terminated bytes and can be passed to imported C pointer parameters on the x86-64 AST compatibility backend. The C importer still represents pointer values as machine-word integers; Swan does not yet have safe pointer types, owned string values, slices, or aggregate ABI lowering.

HTTP/TCP/TLS, socket lifecycle, a complete request/response API, cancellation,
structured task scopes, and Swan-level JSON values are not implemented by
these primitives. Do not use the C worker pool as evidence that Swan async
semantics exist. The next compiler integration work is a length-aware string
and byte-span ABI, followed by opaque runtime handles and aggregate layout.
