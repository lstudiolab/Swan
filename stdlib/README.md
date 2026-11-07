# Swan Standard Library

The standard library is written in Swan wherever the current language can express the feature.

## Foundation

`foundation.swan` is the first standard-library module. It currently provides these implemented functions:

### Output
- `echo(message: string) -> void`
- `echo_int(value: int) -> void`
- `echo_float(value: float) -> void`
- `echo_bool(value: bool) -> void` (prints `1` for true and `0` for false)

### Integer arithmetic
- `abs_int(value: int) -> int`
- `min_int(left: int, right: int) -> int`
- `max_int(left: int, right: int) -> int`
- `clamp_int(value: int, lower: int, upper: int) -> int`
- `sign_int(value: int) -> int` (returns -1, 0, or 1)
- `square_int(value: int) -> int`
- `cube_int(value: int) -> int`
- `power_int(base: int, exponent: int) -> int` (negative exponents return 0)
- `factorial_int(value: int) -> int` (negative inputs return 0)
- `sum_to_int(value: int) -> int` (sums 1 through value; values below 1 return 0)
- `average_int(left: int, right: int) -> float`

### Integer predicates
- `is_even(value: int) -> bool`
- `is_odd(value: int) -> bool`
- `in_range_int(value: int, lower: int, upper: int) -> bool` (inclusive bounds)

These are actual Swan function bodies, not compiler-provided placeholders. They compile into ordinary C functions and can call one another.

## Example

Until module resolution is implemented, combine the library and application source:

```sh
cat stdlib/foundation.swan main.swan > combined.swan
./build/swan combined.swan -o combined.c
cc -std=c11 combined.c -o combined
./combined
```

The compiler parses `import` syntax, but module resolution and linking are not implemented yet. Therefore `import foundation` is not supported. This source-composition workflow is temporary.

## Current limitations

Foundation intentionally stays within Swan's implemented language features. String length/manipulation, input, files, networking, random numbers, and platform APIs need additional string/runtime/compiler support and are not claimed as available here. Integer overflow follows the generated C integer behavior; callers should keep arithmetic within the supported integer range.
