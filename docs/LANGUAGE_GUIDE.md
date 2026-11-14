# The Swan Programming Language
## Language Guide and Reference

**Status:** Written against the current `lstudiolab/Swan` compiler source on the `main` branch.  
**Purpose:** Teach the syntax that exists today, list the complete reserved-word set, and distinguish implemented features from reserved or planned language constructs.

> Swan is an ahead-of-time (AOT) compiled language. Swan source is compiled into target code before the program runs. SVM is Swan's compiler middle-end, not a virtual machine or runtime. There is no interpreter or bytecode dispatch loop in the production execution model.

This guide deliberately does not pretend that every reserved word is already a working language feature. A word can be reserved by the lexer while its syntax or runtime semantics are still incomplete. The status notes below are part of the specification, not optional caveats.

---

## 1. First program

Create a file named `hello.swan`:

```swan
func main() -> int:
    log "Hello, Swan!"
    return 0
```

Build and run it:

```sh
sh build.sh
./build/swanc run hello.swan
```

Build a standalone executable:

```sh
./build/swanc build hello.swan -o hello
./hello
```

The compiler's `run` command builds a host-native executable and launches it as a separate process. It does not interpret the source file.

### Function spelling

- `func` is the canonical, reserved function-declaration keyword.
- `fn` is accepted as a compatibility spelling. It is not reserved; the parser recognizes it contextually at file scope.
- Use `func` in new Swan code and documentation.

Equivalent compatibility example:

```swan
fn main() -> int:
    log "Hello"
    return 0
```

---

## 2. Swan's coding style

Swan source is designed around indentation-based blocks. It does not use braces to delimit function, conditional, or loop bodies.

### 2.1 Indentation

Use four spaces per indentation level. Do not mix tabs and spaces. A block begins after a newline and a deeper indentation level. A colon before the newline is the recommended, canonical form.

```swan
func main() -> int:
    if 10 > 5:
        log "Ten is greater"
    else:
        log "This branch is not selected"

    return 0
```

The parser also accepts an indented block without a colon, but new code should use the colon form consistently.

Rules:

1. Indent every statement in a block by one level.
2. Indent nested blocks by one additional level.
3. Align `else` with its matching `if`.
4. Dedent to close a block; do not write a closing brace.
5. Keep one logical statement per line.
6. Use blank lines to separate logical sections.
7. Put comments on their own line when they explain a block or decision.

The lexer treats indentation as syntax. Inconsistent indentation is an error.

### 2.2 Comments

A hash begins a line comment:

```swan
# This is a comment.
func main() -> int:
    # This is also a comment.
    log "Ready"
    return 0
```

A hash encountered after code also comments out the rest of that line.

### 2.3 Names

Use descriptive names. The lexer accepts ASCII letters, underscores, digits after the first character, and non-ASCII UTF-8 identifier bytes. Identifiers are case-sensitive.

Recommended style:

- Functions: descriptive `snake_case`, such as `calculate_total`.
- Variables: descriptive `snake_case`, such as `item_count`.
- Constants: descriptive names; uppercase is a style choice, not a language rule.
- Avoid names that differ only by capitalization.

The compiler validates UTF-8. Its current identifier handling is byte-oriented and does not implement full Unicode XID classification or Unicode normalization.

### 2.4 Semicolons and line endings

Newlines separate statements. Semicolons are optional statement separators, not required line terminators. Prefer one statement per line and omit semicolons in ordinary Swan code.

---

## 3. Variables and bindings

Swan currently has three declaration spellings:

| Form | Mutability | Initialization |
|---|---|---|
| `let name = value` | Immutable | Value required |
| `const name = value` | Immutable | Value required |
| `var name = value` | Mutable | Initial value optional |

Examples:

```swan
func main() -> int:
    let language = "Swan"
    const version = 1

    var score = 10
    score = score + 5

    log language
    log version
    log score
    return 0
```

Rules:

- Use `let` for a binding that must not be reassigned.
- Use `const` when the binding is also intended to communicate constant intent.
- Use `var` when reassignment is required.
- Reassign a mutable binding with `=`.
- Reassignment of an immutable binding is a semantic error.
- A `var` may be declared without an initializer, but reading it before it is initialized is an error.
- A declaration name may not be redeclared in the same active scope.
- A variable's current implementation tracks scalar-versus-array category. Do not assume that the compiler already enforces a complete static type system for every type annotation.

Optional type annotations use a colon:

```swan
let count: int = 12
var enabled: bool = true
```

The parser accepts a type name after the colon. The existence of a parsed type annotation does not mean every type is fully checked or supported by every backend.

---

## 4. Functions

A function is declared at file scope with `func`, a name, parentheses, optional parameters, an optional return type, and an indented body.

General form:

```swan
func function_name(parameter: type) -> return_type:
    # statements
    return value
```

Example:

```swan
func add(left: int, right: int) -> int:
    return left + right

func main() -> int:
    let result = add(20, 22)
    log result
    return 0
```

Current rules:

- Function names are required.
- Parentheses are required, including for functions with no parameters.
- Parameters are comma-separated.
- Up to 16 parameters are accepted by the current parser.
- Parameter type annotations are optional in the parser.
- The return type annotation after `->` is optional in the parser.
- Function calls must refer to a known function in the current semantic model.
- The number of call arguments must match the function's declared parameter count.
- Functions are declared at file scope; nested function declarations are not part of the current grammar.
- `main` is the program entry point used for reachability and executable generation.

A simple function call:

```swan
func greet():
    log "Welcome"

func main() -> int:
    greet()
    return 0
```

### External declarations

An external declaration describes a function implemented outside the Swan source. The parser accepts the contextual `external` spelling before a function declaration. External declarations have no Swan body.

C header imports are the documented route for supported C interoperability; see [Section 14](#14-c-interoperability).

### Exported C functions

The parser accepts `export C func` as an exported function marker. The currently documented native x86-64 System V scalar/pointer ABI subset is:

```swan
export C func swan_add(left: i64, right: i64) -> i64:
    return left + right
```

This does not imply that every Swan type or every target ABI can be exported.

---

## 5. Output

The current language provides a built-in logging/output statement:

```swan
log "Hello"
log 123
log 3.14
log true
```

The parser also accepts `print expression` and the `echo!(...)` output intrinsic form:

```swan
func main() -> int:
    print "Output through print"
    echo!("Output through echo!")
    return 0
```

The Foundation module provides `echo_int` and `echo_bool` functions. The compiler's `log` statement is a language construct; `echo_int` and `echo_bool` are library functions.

---

## 6. Types and values

The current lexer recognizes these literal categories:

| Literal | Example | Notes |
|---|---|---|
| Integer | `42` | Decimal integer literal |
| Floating-point | `3.14` | Decimal point must have digits on both sides |
| String | `"Swan"` | Single-quoted and double-quoted forms are accepted |
| Boolean | `true`, `false` | Boolean literals |
| Null | `null` | Reserved, but rejected by the current expression parser/runtime |

String escapes currently recognized:

| Escape | Meaning |
|---|---|
| `\\n` | Newline |
| `\\r` | Carriage return |
| `\\t` | Tab |
| `\\\\` | Backslash |
| `\"` | Double quote |
| `\'` | Single quote |

Unknown escapes and unterminated strings are errors. The lexer preserves UTF-8 string bytes.

### Type names

Type names such as `int`, `bool`, `void`, `i64`, and `u32_array` are identifiers, not reserved words. They are written in annotations or compiler/library interfaces.

The compiler's implemented source/backend subsets are not yet identical. In particular, the typed-IR path is currently focused on scalar integer and boolean operations, while the x86-64 compatibility path supports additional established features. Floating-point literal tokenization alone is not proof of complete floating-point type checking or code generation.

Do not assume implicit conversions between unrelated types. Use only conversions explicitly documented by the compiler version and target you are building against; a general cast syntax is not currently specified in this guide.

---

## 7. Operators and expressions

The parser currently recognizes the following operators.

### 7.1 Arithmetic

| Operator | Meaning |
|---|---|
| `+` | Addition |
| `-` | Subtraction; also unary negation |
| `*` | Multiplication |
| `/` | Division |
| `//` | Floor-division token |
| `%` | Remainder |

### 7.2 Comparison

| Operator | Meaning |
|---|---|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

### 7.3 Boolean logic

| Operator | Meaning |
|---|---|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Unary logical negation |

Use the word operators `and` and `or`; `&&` and `||` are not current lexer operators.

### 7.4 Assignment

| Operator | Meaning |
|---|---|
| `=` | Assignment; also used in declarations |

Equality uses `==`, not `=`.

### 7.5 Access and grouping

| Syntax | Meaning |
|---|---|
| `(expression)` | Group an expression |
| `function(arg)` | Function call |
| `array[index]` | Array element access |
| `module.function()` | Dotted call path |
| `value.member` | Member-access syntax is parsed, but general member resolution is not implemented |

### 7.6 Precedence

From highest to lowest, the current parser uses:

1. Parentheses, calls, indexing, and dotted access.
2. Unary `-` and `not`.
3. `*`, `/`, `//`, `%`.
4. `+`, `-`.
5. `<`, `>`, `<=`, `>=`.
6. `==`, `!=`.
7. `and`.
8. `or`.
9. Assignment `=`.

Use parentheses whenever an expression could be read ambiguously.

Example:

```swan
let total = (base + bonus) * multiplier
let allowed = age >= 18 and has_permission
```

The parser accepts assignment as an expression operator, but assignment is semantically restricted to a mutable variable or a supported array element.

---

## 8. Conditional statements

Use `if` and `else`:

```swan
func main() -> int:
    let score = 85

    if score >= 90:
        log "Excellent"
    else:
        log "Keep improving"

    return 0
```

Rules:

- The condition follows `if`.
- The body is an indented block.
- `else` aligns with its `if`.
- An `else` block is optional.
- Nested `if` statements are supported by the parser.
- The parser accepts an `else if` form through a nested conditional.
- The condition and branches must use expressions and statements supported by the active semantic/backend path.

---

## 9. Loops and control transfer

### 9.1 While

`while` repeats its body while its condition is true:

```swan
func main() -> int:
    var count = 0

    while count < 5:
        log count
        count = count + 1

    return 0
```

### 9.2 Break and continue

- `break` exits the nearest enclosing loop.
- `continue` skips the rest of the current loop body and proceeds to the next iteration.
- Both are valid only inside a loop.

```swan
func main() -> int:
    var count = 0

    while count < 10:
        count = count + 1

        if count == 3:
            continue

        if count == 7:
            break

        log count

    return 0
```

### 9.3 Return

`return` exits the current function. A value may follow it:

```swan
func square(value: int) -> int:
    return value * value
```

A bare `return` is also accepted by the parser.

### 9.4 Infinite loops

`loop:` starts an unconditional loop. It uses the same control-flow representation as `while`, so `break` exits the nearest loop and `continue` begins the next iteration. The construct is lowered into shared SVM control flow and is supported by the x86-64, AArch64, ARMv7, RISC-V64, WASM32, and WASM64 backends, subject to each backend's existing scalar type and ABI limits.

```swan
func count_to(limit: int) -> int:
    var count = 0
    loop:
        if count >= limit:
            break
        count = count + 1
    return count
```

`upto` remains reserved; its range-loop grammar and inclusive/exclusive boundary semantics are not yet specified.

---

## 10. Arrays

The x86-64 compatibility backend currently supports zero-initialized `u32` arrays.

Allocation forms:

```swan
var values = array(100)
var values_u32 = array_u32(100)
```

Bounds-checked and unchecked allocation forms:

| Function | Behavior |
|---|---|
| `array(length)` | Checked `u32` array |
| `array_u32(length)` | Checked `u32` array |
| `array_unchecked(length)` | Unchecked `u32` array |
| `array_u32_unchecked(length)` | Unchecked `u32` array |

Indexing and assignment:

```swan
func main() -> int:
    var values = array_u32(4)
    values[0] = 10
    values[1] = 20
    log values[0]
    return 0
```

Rules:

- The allocation length is the single required argument.
- Indexing uses square brackets.
- The current semantic checker requires the base of an indexed expression to be a declared array variable.
- Checked arrays perform bounds checks.
- Unchecked arrays omit per-access bounds checks; the programmer is responsible for proving every index is valid.
- The current documented array implementation is a zero-initialized `u32` array, not a general generic array type.
- Array support is not yet available on every target backend.

Small fixed-size, non-escaping arrays may use stack-frame storage as an optimization. This is an implementation detail; it does not change the indexing contract.

---

## 11. Imports and modules

A standard-library module is imported by name:

```swan
import foundation

func main() -> int:
    echo_int(abs_int(-42))
    return 0
```

The loader resolves `foundation` to `foundation.swan` in the selected standard-library directory. By default, imported Swan modules are bundled into the program's generated assembly. Imported functions are included according to reachability.

A lazy import uses the contextual modifier:

```swan
lazy import foundation
```

Lazy imports use the compiler's optional shared-library module model on the supported native path. The generated `.modules/` directory must remain beside the executable.

The parser accepts `import` contextually; it is not part of the reserved-word set. `lazy` is also contextual and is not reserved.

### Importing C headers

```swan
import C "math_api.h"
```

C interoperability is limited to the documented importer and ABI subset in Section 14.

---

## 12. Foundation standard library

The repository's current `stdlib/foundation.swan` provides:

| Function | Purpose |
|---|---|
| `foundation_hello()` | Prints a Foundation readiness message and returns an integer |
| `echo_int(value)` | Logs an integer |
| `echo_bool(value)` | Logs a boolean |
| `abs_int(value)` | Absolute value |
| `min_int(left, right)` | Smaller of two integers |
| `max_int(left, right)` | Larger of two integers |
| `clamp_int(value, lower, upper)` | Constrains a value to a range |
| `sign_int(value)` | Returns -1, 0, or 1 |
| `square_int(value)` | Squares an integer |
| `cube_int(value)` | Cubes an integer |
| `power_int(base, exponent)` | Integer power for non-negative exponents |
| `factorial_int(value)` | Factorial for non-negative input |
| `sum_to_int(value)` | Sum from 1 through the input |
| `is_even(value)` | Tests whether an integer is even |
| `is_odd(value)` | Tests whether an integer is odd |
| `in_range_int(value, lower, upper)` | Inclusive range check |
| `average_int(left, right)` | Integer average |

These are library functions, not language keywords. Their current implementations are integer-oriented.

---

## 13. Complete reserved-keyword reference

The lexer defines exactly **31 case-sensitive reserved words**:

```text
func struct let var null if else const type match guard block while loop
true false break continue return as from self pass upto init deinit enum
throw throws rethrow catch
```

The table below describes their intended role and current implementation status.

| Keyword | Intended meaning | Current status |
|---|---|---|
| `func` | Declare a function | Implemented; canonical spelling |
| `struct` | Declare a structure type | Reserved; structure declarations are not implemented |
| `let` | Declare an immutable binding | Implemented |
| `var` | Declare a mutable binding | Implemented |
| `null` | Null value literal | Reserved; parser explicitly rejects it |
| `if` | Conditional branch | Implemented |
| `else` | Alternate conditional branch | Implemented |
| `const` | Immutable/constant binding | Implemented as immutable declaration |
| `type` | Type declaration | Reserved; type declarations are not implemented |
| `match` | Pattern matching | Reserved; not implemented |
| `guard` | Guard-style conditional validation | Reserved; not implemented |
| `block` | Explicit block construct | Reserved; not implemented as a separate construct |
| `while` | Condition-controlled loop | Implemented |
| `loop` | Unconditional loop | Implemented as `loop:`; uses shared control-flow IR |
| `true` | Boolean true literal | Implemented |
| `false` | Boolean false literal | Implemented |
| `break` | Exit nearest loop | Implemented |
| `continue` | Continue nearest loop | Implemented |
| `return` | Return from function | Implemented |
| `as` | Conversion or type-ascription operator | Reserved; no general cast syntax is specified |
| `from` | Import/source qualifier | Reserved; no standalone syntax is implemented |
| `self` | Current instance reference | Tokenized; instance/member semantics are not implemented |
| `pass` | Empty/no-op statement | Implemented |
| `upto` | Upper-bound loop/range word | Reserved; not implemented |
| `init` | Initializer declaration | Reserved; not implemented |
| `deinit` | Deinitializer declaration | Reserved; not implemented |
| `enum` | Enumeration declaration | Reserved; not implemented |
| `throw` | Raise an error/exception | Reserved; exception semantics are not implemented |
| `throws` | Mark a throwing function | Reserved; exception semantics are not implemented |
| `rethrow` | Propagate a caught exception | Reserved; exception semantics are not implemented |
| `catch` | Handle an exception | Reserved; exception semantics are not implemented |

### Contextual words (not reserved)

These spellings are recognized in specific parser contexts and remain available as ordinary identifiers elsewhere:

| Spelling | Contextual use |
|---|---|
| `fn` | Compatibility spelling for a file-scope function declaration |
| `import` | File-scope module or C-header import |
| `export` | File-scope exported function marker |
| `C` | C import/export marker |
| `external` | External function declaration marker |

| `lazy` | Modifier before an import |
| `out` | Legacy block terminator accepted after a dedent |
| `log` | Built-in output statement spelling |
| `print` | Compatibility output statement spelling |
| `echo` | Used with `!` for the output intrinsic |

The following are **not** reserved keywords: `int`, `bool`, `void`, `float`, `i64`, `u32`, `free`, `manual`, `defer`, `fn`, `import`, and `export`. Some may be recognized by type annotations, libraries, or contextual parser logic, but they are identifiers at the lexer level.

---

## 14. C interoperability

The current C header importer uses Clang's C AST dump to read a supported subset of declarations. Imported functions become external declarations and native x86-64 System V calls.

Example:

```swan
import C "math_api.h"

func main() -> int:
    return c_add(20, 22)
```

The current documented subset includes:

- Up to 16 scalar integer/boolean parameters.
- Scalar integer/boolean or `void` returns.
- Opaque pointer parameters and returns represented as machine-word `i64` values on x86-64 System V.
- Exported Swan scalar/pointer functions using the documented native C ABI.

Current limitations include structs/unions passed by value, variadic functions, callback invocation, floating-point ABI classification, C++ name mangling, non-System-V calling conventions, and dereferencing imported opaque pointers from Swan. Clang must be installed when using C header imports. C source bundling/linker inputs are currently limited to the native x86-64 path.

---

## 15. Errors, warnings, and compilation guarantees

Swan's compiler is phase-gated. A failed source read, lex, parse, import, semantic analysis, IR verification, optimization, code-generation, assembly, or link step stops the build. Later phases must not consume invalid output from an earlier phase.

Examples of errors include:

- Invalid UTF-8.
- Unterminated strings or unknown escape sequences.
- Inconsistent indentation.
- Missing function names, parentheses, or block indentation.
- Unknown variables or function names.
- Reading an uninitialized variable.
- Reassigning an immutable binding.
- Calling a function with the wrong argument count.
- Assigning to something other than a supported mutable variable or array element.
- Using `break` or `continue` outside a loop.
- Using unsupported syntax or a target operation that the selected backend cannot generate.

The compiler also reports non-fatal warnings for suspicious constructs, including self-assignment, literal division by zero, constant conditions, potentially non-terminating constant-true loops, and source functions unreachable from `main`.

Promote warnings to errors:

```sh
./build/swanc build hello.swan --warnings-as-errors -o hello
```

Warnings are conservative indicators, not proof of a bug. A warning promoted to an error stops compilation before code generation.

---

## 16. Performance model

Swan is designed as a compiled systems language. Its performance model depends on the generated target code, not on interpreting Swan instructions.

The production model is:

1. Read and validate source.
2. Tokenize and parse.
3. Resolve imports and declarations.
4. Perform semantic analysis.
5. Lower source into Swan-owned intermediate representations.
6. Verify the IR.
7. Optimize the IR.
8. Select the destination backend.
9. Legalize and select target instructions.
10. Allocate registers and lay out stack frames.
11. Emit assembly/object code.
12. Link a native executable when requested.
13. Execute the resulting native program as a normal operating-system process.

SIL and SVM are compiler representations. SVM is not a CPU, runtime, interpreter, or bytecode format. The executable must not need a Swan VM.

Optimization is required to preserve the language's semantics. Integer transformations must respect width, signedness, and overflow rules. Memory and call transformations must preserve observable side effects. Backends must reject unsupported operations rather than emit placeholders.

### Target availability

The repository currently documents these target families with unequal feature coverage:

- x86-64 System V Linux: native executable/assembly, typed-IR scalar path plus compatibility path.
- AArch64 Linux: Swan-owned ELF assembly/object for a limited scalar IR subset.
- ARMv7 Linux (ARM state): limited `i32`/`i1` subset.
- RISC-V64 Linux: limited scalar IR subset.
- WebAssembly 32/64: module emission for the implemented subset; a WebAssembly host is required to execute it.
- Optional LLVM target-triple bridge: explicit compatibility path, not the default Swan-owned backend.

An emitted object is not proof of full language support, ABI completeness, successful linking, or runtime correctness. Check the target-specific support before relying on a feature.

---

## 17. Practical complete example

```swan
import foundation

func calculate_total(price: int, quantity: int) -> int:
    return price * quantity

func main() -> int:
    let unit_price = 25
    var quantity = 3
    var total = calculate_total(unit_price, quantity)

    if total >= 100:
        log "Large order"
    else:
        log "Standard order"

    var counter = 0
    while counter < quantity:
        log counter
        counter = counter + 1

    echo_int(total)
    return 0
```

This example uses the currently established function, binding, call, arithmetic, conditional, loop, output, and Foundation mechanisms. It intentionally avoids reserved-but-unimplemented features.

---

## 18. Current language boundary and future evolution

The reserved-word table is the authoritative lexer keyword list, not a promise that every language feature has shipped. The current parser and semantic checker implement a smaller subset. The current backends also have different feature coverage.

Do not write production code that depends on these reserved-but-unimplemented features:

- Structure declarations and general structure/member access.
- Type declarations, enums, and pattern matching.
- General casts using `as`.
- Null values and general nullable types.
- Initializers and deinitializers.
- Exception handling with `throw`, `throws`, `rethrow`, and `catch`.
- Guard statements and the dedicated `upto` range-loop construct.
- General ownership/borrowing semantics.

A language feature is complete only when the lexer, parser, semantic analysis, IR lowering, verifier, relevant backend(s), diagnostics, and tests agree. Until then, the compiler must diagnose unsupported use rather than silently changing its meaning.

For compiler architecture and implementation requirements, see:

- [Compiler System Contract](COMPILER_SYSTEM.md)
- [Compiler Engineering Style](ENGINEERING_STYLE.md)
- [SVM Compiler Architecture](SVM_ARCHITECTURE.md)
