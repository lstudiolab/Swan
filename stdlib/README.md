# Swan Standard Library

The standard library is implemented in Swan source under `stdlib/`. Import a module explicitly, then call its public functions with the `std:` namespace:

```swan
import std
import std_math

func main() -> int:
    std:print >> std:abs(-42)
    std:print >> std:gcd(84, 30)
    return 0
```

`std:print >> expression` is a compiler output intrinsic. All other functions below are ordinary Swan functions loaded from the imported module source.

## Implemented Swan modules

### `std` — core integer helpers

Source: `stdlib/std.swan`

- `abs`, `min`, `max`, `clamp`, `sign`
- `square`, `cube`, `power`, `factorial`, `sum_to`
- `is_even`, `is_odd`, `in_range`, `average`
- `print` output statement: `std:print >> value`

### `std_math` — integer mathematics

Source: `stdlib/std_math.swan`

- Number theory: `gcd`, `lcm`, `is_prime`, `is_composite`
- Division and remainder: `floor_div`, `ceil_div`, `euclidean_mod`
- Integer roots and sequences: `integer_sqrt`, `fibonacci`, `triangular`
- Integer utilities: `distance_int`, `is_power_of_two`, `next_power_of_two`

Import `std_math` to use these functions. The module uses only Swan integer expressions, conditions, loops, local bindings, and direct function calls.

## Compatibility and correctness boundaries

The current language/backend subset is not yet sufficient to implement all Swift and Goose standard-library surfaces in Swan source. The following capabilities must not be advertised as implemented until Swan has the required language/runtime support and end-to-end tests:

- Generic typed collections, dictionaries, sets, iterators, slices, and collection protocols.
- General-purpose strings, Unicode scalar/grapheme processing, formatting, and parsing.
- Floating-point math, transcendental functions, and IEEE-754 classification.
- File and directory I/O, processes, environment access, networking, and sockets.
- JSON, CSV, Base64, regular expressions, and general binary buffers.
- Safe pointers, allocators, ownership/borrowing, and memory layout APIs.
- Tasks, async sequences, actors, synchronization, and cancellation.
- Reflection, key paths, runtime metadata, and serialization protocols.
- Graphics, audio, physics, and UI frameworks.

These are part of the implementation target, not fake stubs. Each module will be added only when its Swan implementation can compile and its behavior can be tested on the supported targets. Swift Foundation and Goose's platform/game modules are not equivalent to Swift's core `Swift` module.

## Build and test

```sh
sh build.sh
sh tests/std-library.sh
sh tests/std-math.sh
```

The complete suite runs both standard-library tests through `sh test.sh`. Native standard-library import and linking currently use the x86-64 System V compatibility backend. Do not assume standard-library support on cross-target backends until target-specific import, linking, and runtime tests pass.
