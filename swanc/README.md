# swanc — Swan Compiler 2

\`swanc\` is the self-hosted compiler implementation. Compiler 1 remains in \`compiler/\` and is the bootstrap compiler until Compiler 2 can compile itself and pass the same regression suite.

## Required pipeline

\`\`\`text
Swan source
  -> UTF-8 source buffer
  -> Lexer
  -> Parser
  -> AST validation
  -> semantic analysis
  -> SVM typed IR generation
  -> IR verification
  -> optimization
  -> target lowering
  -> target code generation
  -> object / assembly / WebAssembly emission
  -> linker (where the target requires it)
\`\`\`

SVM is the target-independent compilation engine. It is not a CPU target, bytecode interpreter, or runtime execution loop. Compilation must fail with a diagnostic when a construct is invalid or unsupported by the selected lowering/backend.

## Source organization

Subsystems own their files; do not put the compiler in one enormous source file.

- \`lexer/Lexer.swan\` — the lexer remains one cohesive source file.
- \`parser/Parser.swan\` — the parser remains one cohesive source file.
- \`ast/\` — AST representation, traversal, and structural validation.
- \`sema/\` — name resolution, type checking, control-flow checks, and ownership checks.
- \`ir/\` — typed, target-independent SVM IR and its verifier.
- \`silgen/\` — lowering from validated AST into SVM IR.
- \`optimization/\` — verified optimization passes, including constant folding and dead-code elimination.
- \`targets/x86_64/\`, \`aarch64/\`, \`armv7/\`, \`riscv64/\`, \`wasm32/\` — target-specific lowering and emission.
- \`driver/\` — command-line orchestration, source loading, diagnostics, and build modes.
- \`linker/\` — object and executable linking orchestration.
- \`tests/\` — Compiler 2 unit, regression, negative, and bootstrap tests.

Only add a subsystem file when implementing its behavior. Do not add placeholder modules or speculative features.

## Current Compiler 2 implementation

- Lexer: scans UTF-8 source bytes into caller-owned token storage. Token records use five words: kind, byte offset, byte length, starting line, and starting column. Keyword recognition is hardcoded, exact, and case-sensitive.
- Parser: currently parses integer expressions using Pratt precedence. It is not yet a complete source-file parser.
- AST: defines the existing five-word expression-node layout and validates node kinds, token bounds, child references, and root placement.
- SVM IR: defines an initial eight-word linear instruction format, typed opcodes, SSA result numbering, and structural/use-before-definition verification.
- Optimization: contains an initial integer i32 constant-folding pass for add, subtract, and multiply. This is not yet the complete optimizer.

These are real implementation slices, not a claim that Compiler 2 is complete. The C compiler remains the behavioral reference for language semantics, diagnostics, IR invariants, optimizations, target contracts, and tests.

## Lexer contract

\`swan_lex(source, source_length, tokens, token_capacity, diagnostic)\` accepts UTF-8 bytes stored as integer elements in a \`u32_array\`. The caller owns every buffer. Each token occupies five words: token kind, byte offset, byte length, starting line, and starting column. The diagnostic buffer contains error code, byte offset, line, and column. Success returns the token count including EOF; a negative result indicates failure. Token kinds are aligned with \`compiler/include/swan/core.h\`.

The lexer and parser must remain single cohesive files. All other compiler subsystems should be decomposed into focused files within their subsystem directories.

## Bootstrap contract

1. Compiler 1 builds Compiler 2 from the Swan sources in \`swanc/\`.
2. Compiler 2 compiles its own complete source tree.
3. The resulting stage-2 compiler rebuilds the same source tree.
4. Stage-2 and stage-3 outputs must be deterministic or have explicitly normalized metadata, and their regression results must match.
5. Compiler 1 is retained until this process is repeatable and verified.

A successful build of a partial Compiler 2 is not proof of self-hosting. Self-hosting is achieved only when Compiler 2 compiles all of its own implementation without falling back to C for compiler stages.
