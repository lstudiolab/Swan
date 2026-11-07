# Swan Standard Library

The standard library is written in Swan wherever the current language can express the feature.

## Foundation

foundation.swan is the first standard-library module. It provides echo, echo_int, echo_float, echo_bool; integer helpers abs_int, min_int, max_int, clamp_int, sign_int, square_int, cube_int, power_int, factorial_int, sum_to_int, average_int; and predicates is_even, is_odd, in_range_int.

These are actual Swan function bodies, not compiler-provided placeholders. Boolean output currently prints 1 for true and 0 for false. Negative power exponents and negative factorial inputs return 0.

## Example

    fn main():
        echo("Foundation ready")
        echo_int(square_int(5))
        return 0

Swan blocks use indentation; no curly braces or end keyword are used.

Until module resolution is implemented, combine library and application source:

    cat stdlib/foundation.swan main.swan > combined.swan
    ./build/swan combined.swan -o combined.c
    cc -std=c11 combined.c -o combined
    ./combined

The compiler parses import syntax, but module resolution and linking are not implemented. Therefore import foundation is not supported.

## Current limitations

String manipulation, input, files, networking, random numbers, and platform APIs need additional runtime/compiler support. Integer overflow follows generated C integer behavior.
