# Swan WASM64 runtime

Swan executes its WebAssembly 64-bit modules through the Wasmtime runtime engine.
The Swan CLI owns the runtime entry point and enables the WebAssembly memory64
proposal explicitly; Wasmtime supplies the validated WebAssembly execution
engine, bounds-checked linear memory, traps, and host process integration.

## Requirements

- Wasmtime CLI (v50.0.2 or newer recommended)
- A host supported by the installed Wasmtime build

Install Wasmtime using the official instructions:
https://docs.wasmtime.dev/cli-install.html

## Run a compiled Swan module

```sh
swanc build app.swan --target wasm64 -o app.wasm
swan run app.wasm
```

The runtime invokes the module's exported `main` function. Integer return values
are printed by Wasmtime's `--invoke` interface. The module is executed with
`memory64=y`, so modules using 64-bit linear memory are accepted.

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
