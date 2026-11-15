# Swan Library Ecosystem

**Status:** foundational source-module search paths are implemented; this document separates those working capabilities from the package and module-system work still required for a large library ecosystem.

## Working today

- A Swan module is currently one `<name>.swan` file in a module search directory.
- `import name` loads that source module.
- The configured standard-library root is searched first.
- Repeatable `--module-path <directory>` and `-I <directory>` options add project or vendor roots in command-line order.
- Imports inside imported modules are resolved transitively against the same search path.
- Imported functions participate in the current program's semantic analysis and reachability pass.
- Default builds bundle reachable imported functions into the output program.
- `--lazy-imports` is an existing optional x86-64 shared-module mode, not a general cross-target package ABI.

Example layout:

```text
project/
  app/
    main.swan
  modules/
    app_support.swan
  vendor/
    numeric_helpers.swan
```

```sh
./build/swanc build project/app/main.swan \
  --module-path project/modules \
  --module-path project/vendor \
  -o build/app
```

The standard-library root can be selected with `SWAN_STDLIB` or `--stdlib <directory>`. It is always searched before additional module roots.

## Current hard limits

The current frontend representation is not yet sized for an unbounded library graph:

| Resource | Current limit |
|---|---:|
| Imported module declarations | 64 |
| Loaded modules per program | 64 |
| Functions per program | 1,024 |
| Function parameters | 16 |
| C header imports | 64 |
| Additional module search roots | 63 plus the primary stdlib root |

These limits are enforced by fixed-size compiler data structures. Do not describe the current implementation as supporting unlimited modules or massive dependency graphs. Raising only the numeric constants is not the long-term fix; the owning arrays, symbol index, reachability data, parser storage, and cleanup paths must move together to checked dynamic storage.

## Required architecture for a production library ecosystem

### 1. Source manager

- Preserve each source file as a distinct source buffer with a stable source ID.
- Keep token and AST source ranges tied to their original file.
- Report diagnostics with file path, line, column, and import stack.
- Avoid concatenating unrelated files into one synthetic source buffer.
- Cache source reads by canonical path and content digest.

### 2. Dynamic module graph

- Replace fixed import/module/function arrays with checked growable vectors.
- Use canonical module identities, not only bare file names.
- Track states such as discovered, loading, loaded, failed, and cached.
- Detect import cycles and report the complete cycle chain.
- Deduplicate equivalent paths and module identities.
- Support deterministic dependency ordering and graph traversal.
- Enforce configurable resource budgets rather than silent fixed ceilings.

### 3. Namespaces and visibility

- Resolve qualified names against the imported module identity.
- Prevent two libraries from accidentally exporting the same unqualified function name.
- Define explicit public and private declarations.
- Keep internal symbols hidden from downstream modules.
- Validate duplicate exports and ambiguous imports.
- Make symbol identity independent of source-file parse order.

### 4. Package manifests and reproducibility

Introduce a documented project manifest (proposed name: `swan.toml`) with:
- package name and version;
- Swan compiler compatibility range;
- library and executable targets;
- source roots;
- dependency declarations;
- target/platform constraints;
- build features;
- exported module roots.

A package resolver must produce a committed lockfile containing exact package versions and content hashes. Resolution must be deterministic, reject dependency conflicts, and support offline builds from a local cache. A manifest format alone is not a package manager; resolution, fetching, integrity verification, and lockfile enforcement must all work before the feature is advertised.

### 5. Library compilation and ABI

- Define a stable Swan library interface format with compiler version, target triple, ABI version, exported symbols, type/layout metadata, and dependency identities.
- Compile libraries independently from applications.
- Reuse compatible compiled artifacts from a content-addressed build cache.
- Rebuild dependents when public interface hashes change, not when private implementation details change.
- Keep target-specific artifacts separate.
- Define native static and shared library linking without relying on the current experimental lazy-import resolver.
- Make cross-target dependency resolution select artifacts for the exact target and ABI.

### 6. Standard library organization

The standard library should be a set of independently testable modules grouped by capability, not a single monolithic file. Candidate families include core, numeric, collections, strings, filesystem, process, networking, serialization, concurrency, testing, graphics, audio, and UI. A family is implemented only when its required language features, runtime support, backend coverage, and tests exist. Platform-dependent APIs must state their platform contract.

### 7. Build and package tooling

The eventual CLI should provide distinct operations for:
- initializing a package;
- adding/removing dependencies;
- resolving and verifying dependencies;
- building a package or workspace;
- testing a package;
- listing dependency and feature graphs;
- inspecting package metadata;
- publishing and fetching versioned packages.

These operations must use the same resolver and manifest rules as direct compiler builds. Publishing must never happen implicitly during a build.

### 8. Quality gates

Every library must have:
- a declared public API;
- unit and integration tests;
- dependency and target declarations;
- deterministic build inputs;
- documented platform support;
- ABI compatibility checks for released binary interfaces;
- license and provenance metadata for redistributed code.

CI should test a multi-package workspace, transitive imports, duplicate names, missing dependencies, cycles, version conflicts, cache invalidation, offline resolution, and at least one independently compiled library linked into a separate executable.

## Implementation order

1. **Source identity and dynamic storage:** remove fixed module/function limits and preserve per-file diagnostics.
2. **Module identity and namespaces:** qualified resolution, visibility, cycle diagnostics, deterministic graph.
3. **Manifest and lockfile:** reproducible local workspaces and dependency resolution.
4. **Independent library artifacts:** stable interface metadata and target-aware linking.
5. **Build cache and CLI:** incremental compilation, offline builds, package commands.
6. **Registry and publishing:** integrity, provenance, version policy, and secure distribution.

Do not skip ahead to a public registry before module identity, interfaces, lockfiles, and reproducible builds are stable.
