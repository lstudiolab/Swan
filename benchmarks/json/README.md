# JSON runtime benchmark

Run from the repository root after installing Go, Rust/Cargo, and Goose:

```sh
sh build.sh
GOOSE=/path/to/goose python3 benchmarks/json/run.py
```

The GitHub Actions workflow `.github/workflows/json-runtime-benchmark.yml` runs
the same harness on Ubuntu 24.04 and uploads raw JSON plus a Markdown summary.

Each implementation processes the same nested request 100,000 times per run:
parse the request, validate `id` and `workers`, then compact-encode the parsed
document. Seven timed runs rotate implementation order. Every run must produce
the same checksum and input-plus-output byte count or the benchmark aborts.

- **Swan:** the Swan source calls a C11 JSON runtime driver through the x86-64 C ABI.
  The driver reuses one document handle and its token/output capacities; its hot loop runs in C, so this isolates runtime throughput and excludes Swan loop/FFI-call overhead.
- **Go:** `encoding/json` into a typed struct and back to JSON.
- **Goose:** its standard-library JSON DOM parser and JSON writer.
- **Rust:** `serde_json` with insertion-order preservation.

This is an in-process parse/encode benchmark, not an HTTP server benchmark. It
does not include sockets, TLS, request scheduling, or concurrent load. The Swan
result measures its native runtime path, not a parser written entirely in Swan.
The reported MiB/s counts input bytes plus compact output bytes. Throughput
percentiles are across whole benchmark runs, not per-request latency percentiles.
