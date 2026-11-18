# Swan WASM64 runtime

Swan's portable compilation artifact is a WebAssembly 64-bit module. The Swan
Integrated Compilation layer (SIC) detects the host when that module is run:

- In a browser/WebAssembly host, the module stays WebAssembly and the browser's
  WebAssembly engine executes it. SIC does not recompile it.
- On native x86-64 and ARM64 hosts, SIC invokes Wasmtime's runtime. Wasmtime
  detects the host and JIT-compiles WebAssembly into native machine code for
  that CPU before execution. A temporary WASM input is removed only after the
  runtime completes successfully.
- If host detection or native compilation is unavailable, the original WASM
  module is retained and the normal Wasmtime WASM execution path is attempted.

Wasmtime supplies validation, native JIT code generation, memory64 execution,
traps, and host integration. Swan does not produce a separate ARM64 or x86-64
build artifact for this execution path.

## Requirements

- Wasmtime CLI (v50.0.2 or newer recommended)
- A host supported by the installed Wasmtime build

Install Wasmtime using the official instructions:
https://docs.wasmtime.dev/cli-install.html

## Compile and run a portable Swan module

```sh
# Compile source ahead of time to portable WASM64
swanc compile app.swan -o app.wasm

# SIC detects the host and selects native compilation or WASM execution
swan run app.wasm

# Compile and execute in one step
swan run app.swan --target wasm64
```

On native hosts, Wasmtime JIT-compiles the module for the detected CPU during
execution. An explicitly supplied `.wasm` file is retained so it remains
portable. Browser deployments use the original `.wasm` file and
`browser.js`; they do not use Swan's native execution path.

The runtime invokes the module's exported `main` function. Integer return values
are printed by Wasmtime's `--invoke` interface. Wasmtime JIT-compiles and
executes the module with `memory64=y`, so modules using 64-bit linear memory
are accepted.

For browser hosts, import `runtime/wasm64/browser.js` and call
`runSwanWasm("/path/to/app.wasm")`. The browser engine owns WebAssembly
validation, compilation, and execution; Swan does not emit native machine code
or run a Swan JIT in the browser.

Set `SWAN_WASM_ENGINE` to use a Wasmtime-compatible executable at a custom
path:

```sh
SWAN_WASM_ENGINE=/opt/wasmtime/bin/wasmtime swan run app.wasm
```

## Current runtime boundary

- Swan's current WASM backend emits a limited scalar/control-flow subset.
- The runtime currently invokes an exported zero-argument `main`; arguments are
  forwarded to Wasmtime's invoke interface but source-level argv support is not
  implemented in the compiler.
- WASI imports are provided by Wasmtime where supported. The current Swan
  backend does not yet lower Swan `print`, general external imports, or the
  complete standard library to WASI host calls.
- The runtime engine is Wasmtime, not a Swan-authored WebAssembly interpreter.
  This avoids introducing an incomplete or unsafe bytecode engine into Swan.
- Runtime engine installation is separate from the Swan compiler installation.

The runtime integration is considered supported only for modules that the Swan
backend can emit and Wasmtime can instantiate with memory64 enabled.
