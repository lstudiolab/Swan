# Swan WASM64 runtime

Swan's portable compilation artifact is a WebAssembly 64-bit module. The Swan
Integrated Compilation layer (SIC) detects the host when that module is run:

- In a browser/WebAssembly host, the module stays WebAssembly and the browser's
  WebAssembly engine executes it. SIC does not recompile it.
- On supported native x86-64 and ARM64 hosts, SIC asks Wasmtime to compile the
  module into a host-specific `.cwasm` artifact. Wasmtime's Cranelift code
  generator selects the host machine architecture. The portable input is only
  removed when it was a temporary compiler output and native compilation
  succeeded.
- If host detection or native compilation is unavailable, the original WASM
  module is retained and the normal Wasmtime WASM execution path is attempted.

Wasmtime supplies validation, memory64 execution, traps, and host integration.
The `.cwasm` artifact is Wasmtime-specific and host-specific; it is not a
standalone operating-system executable.

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

On native hosts, the first run also creates an architecture-specific
`app.wasm.cwasm.x86_64` or `app.wasm.cwasm.arm64` cache beside a persistent
WASM input. Browser deployments use the original `.wasm` file and
`browser.js`; they do not use the native cache.

The runtime invokes the module's exported `main` function. Integer return values
are printed by Wasmtime's `--invoke` interface. The module is compiled and
executed with `memory64=y`, so modules using 64-bit linear memory are accepted.

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
