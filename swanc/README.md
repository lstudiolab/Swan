# swanc — Swan Compiler 2

Compiler 2 is Swan's self-hosted compiler. It adopts the architectural separation of the Swift compiler: parsing and semantic analysis produce a typed AST; SILGen lowers that AST into high-level, ownership-aware SIL; SIL analyses and optimizations operate before IRGen; IRGen lowers SIL into Swan's target-independent machine IR; SVM generation and optimization prepare it for target lowering, code generation, and linking.

This is an architectural model, not a dependency on Swift or LLVM. Swan owns its language semantics, SIL, IR, SVM, optimizers, and backends. The objective is a capable, maintainable, multi-target compiler—not an arbitrary source-line-count contest with Swift. Swift's compiler is a very large mature project; Swan should exceed it only where we have concrete language or backend capabilities, not by adding empty files.

Compiler 1 remains in `compiler/` as the bootstrap compiler and behavioral reference until Compiler 2 can compile itself and pass the same regression suite.

## Compiler pipeline

```text
.swan source
  -> driver
  -> lexer -> parser -> AST -> AST verifier
  -> semantic analysis (modules, scopes, lookup, types, inference,
     control flow, ownership)
  -> SILGen -> SIL verifier -> SIL demand analysis -> SIL pass manager
  -> SIL optimization
  -> IRGen -> IR builder -> IR verifier
  -> SVM generation -> SVM verifier -> SVM pass manager
  -> SVM optimization
  -> target triple / ABI / data layout
  -> instruction legalization -> register allocation
  -> assembly or object emission
  -> linker -> executable / library / WebAssembly
```

SVM is Swan's target-independent machine-level compilation representation. It is not an interpreter, a runtime execution loop, or a CPU target. The backend is responsible for target-specific lowering. Unsupported syntax, semantic constructs, IR operations, or target instructions are hard compilation errors.

## Source organization

The implementation is split into focused source files. Each file owns one data model, analysis, pass manager, or transformation boundary. The lexer and parser live together in `Parse/` as separate cohesive single files; their complexity belongs in their implementations rather than being scattered across tiny files.

```text
swanc/
├── driver/
│   ├── driver.swan
│   ├── Arguments.swan
│   ├── Diagnostics.swan
│   └── CompilationSession.swan
├── Parse/
│   ├── Lexer.swan
│   └── Parser.swan
├── ast/
│   ├── AST.swan
│   ├── ASTNode.swan
│   ├── ASTVerifier.swan
│   ├── SourceLocation.swan
│   ├── Expressions.swan
│   ├── Statements.swan
│   ├── Declarations.swan
│   └── TypeSyntax.swan
├── sema/
│   ├── sema.swan
│   ├── ModuleLoader.swan
│   ├── Scopes.swan
│   ├── Symbols.swan
│   ├── NameLookup.swan
│   ├── Types.swan
│   ├── TypeChecker.swan
│   ├── TypeInference.swan
│   ├── ControlFlow.swan
│   └── Ownership.swan
├── sil/
│   ├── Silgen.swan
│   ├── SILModule.swan
│   ├── SILFunction.swan
│   ├── SILValues.swan
│   ├── SILInstructions.swan
│   ├── SILBasicBlock.swan
│   ├── SILVerifier.swan
│   ├── Sildemand.swan
│   ├── SILDemandAnalysis.swan
│   ├── SILPassManager.swan
│   └── SilOptimization.swan
├── ir/
│   ├── IR.swan
│   ├── IRGen.swan
│   ├── IRTypes.swan
│   ├── IRInstructions.swan
│   ├── IRBuilder.swan
│   └── IRVerifier.swan
├── svm/
│   ├── SvmGen.swan
│   ├── SvmOptimization.swan
│   ├── SVMInstructionSet.swan
│   ├── SVMRegisters.swan
│   ├── SVMControlFlow.swan
│   ├── SVMVerifier.swan
│   └── SVMPassManager.swan
├── backend/
│   ├── targetbackend.swan
│   ├── codegen.swan
│   ├── TargetTriple.swan
│   ├── TargetABI.swan
│   ├── TargetDataLayout.swan
│   ├── InstructionLegalizer.swan
│   ├── RegisterAllocator.swan
│   ├── AssemblyEmitter.swan
│   └── ObjectEmitter.swan
├── linker/
│   ├── linker.swan
│   ├── ToolchainDiscovery.swan
│   └── LinkArguments.swan
└── optimization/
    ├── ConstantFold.swan
    ├── UseCountAnalysis.swan
    ├── DeadCodeElimination.swan
    └── Optimization.swan
```

## Component boundaries

- **Driver:** CLI, build modes, target selection, source loading, diagnostics, output paths, and orchestration only.
- **Lexer / parser:** lexical and grammatical analysis. Both remain one cohesive file.
- **AST:** source syntax nodes, source locations, traversal, and structural verification.
- **Sema:** module loading, scopes, symbol resolution, canonical types, type checking, inference, control-flow checks, and ownership/borrowing.
- **SIL:** typed SSA representation with explicit basic blocks, effects, and ownership. SILGen must consume only semantically valid AST.
- **SIL analyses and passes:** demand/liveness analysis, pass scheduling, and high-level optimization. Preserve side effects, traps, and ownership semantics.
- **IRGen / IR:** lower verified SIL into Swan's lower-level target-independent IR. This is Swan's analogue of Swift IRGen; it does not emit LLVM IR.
- **SVM:** generate and optimize Swan's target-independent machine-level representation. It contains no physical CPU register names or target opcodes.
- **Backend:** target triples, ABIs, data layout, legalization, register allocation, and target-specific emission.
- **Linker:** toolchain discovery, safe argument construction, and final artifact linking.

A stage may consume only the preceding stage's defined output. Every representation has a verifier. Every transformation must preserve observable behavior and rerun the relevant verifier. A file is not considered implemented merely because it exists: it must contain working code, tests, and integration into its owning stage.

## Implementation status

Compiler 2 is still early and is **not yet a complete or executable compiler pipeline**. The newly separated component files currently document implementation boundaries and invariants; they are not all implemented algorithms and are not wired into a working driver.

Existing implementation slices:
- Lexer: UTF-8 scanning into caller-owned token storage, with five-word token records.
- Parser: Pratt parsing for integer expressions; not yet a complete source-file parser.
- AST: initial five-word expression-node representation and structural validation.
- IR: initial eight-word linear typed instruction representation, SSA result numbering, and structural/use-before-definition verification.
- Optimization: a verified linear-IR pass-manager slice with conservative i32 add/subtract/multiply constant folding. The pass manager verifies input and output; it is not yet a complete optimizer.
- Ownership: an executable linearized ownership-event verifier checks initialization, reads/writes, shared and exclusive borrows, borrow closure, moves, drops, and use-after-move/drop. It rejects malformed event streams and reports the first failing event. It is not yet connected to AST/CFG semantic analysis, does not infer borrow regions or insert drops, and is not a complete Rust-style ownership implementation.
- Validation: `tests/swanc-optimizer.swan` exercises 1,024 constant-folding cases, and `benchmarks/swanc_optimizer.py` measures the verified optimizer slice. The benchmark explicitly runs Compiler 2 optimizer source through the Compiler 1 bootstrap; it is not a full Compiler 2 or self-hosted compiler benchmark.

The ownership regression harness in `tests/swanc-ownership.sh` compiles the Swan-authored verifier with Compiler 1 and exercises valid and invalid ownership traces. The component breakdown is an implementation map, not a claim of feature completeness. Implement each component with concrete types, functions, invariants, unit tests, negative tests, and integration before enabling it in the driver.

## Targets

The architecture must support multiple independent target backends, including x86-64, AArch64, ARM, RISC-V, and WebAssembly. A target is supported only when its ABI, data layout, legalization, register allocation (where applicable), emitter, object format, and tests are implemented. A target name alone does not constitute support.

## Bootstrap contract

1. Compiler 1 builds Compiler 2 from the Swan sources in `swanc/`.
2. Compiler 2 compiles its complete source tree without invoking C implementations for compiler stages.
3. The stage-2 compiler rebuilds the same source tree.
4. Stage-2 and stage-3 outputs are deterministic, or metadata is normalized before comparison.
5. Both stages pass the same positive, negative, regression, and target tests.
6. Keep Compiler 1 until bootstrap is repeatable and verified.

A successful build of a partial Compiler 2 is not proof of self-hosting.
