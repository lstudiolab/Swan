# Swan Standard Library

The standard library is written in Swan and lives under `stdlib/`. Imported modules are parsed and semantically checked during compilation. Runtime-lazy module binaries are generated only for imported functions reachable from `main`; a cached resolver loads each required module when its first imported function is called.

## Foundation

Import Foundation with `import foundation`. The current module provides:

- Output helpers: `echo_int`, `echo_bool`, and the `echo!(...)` intrinsic.
- Integer helpers: `abs_int`, `min_int`, `max_int`, `clamp_int`, `sign_int`, `square_int`, `cube_int`, `power_int`, `factorial_int`, `sum_to_int`, `average_int`, `is_even`, `is_odd`, and `in_range_int`.
- `foundation_hello`, a small module smoke-test function.

`echo!("text")` is compiler-recognized for string literals and emits a direct `puts` call. It does not allocate a general heap string or format the value through `printf`.

```swan
import foundation

fn main():
    echo!("Hello from Swan")
    echo_int(abs_int(-42))
    return 0
out
```

Build with `swan build main.swan -o app`. Set `SWAN_STDLIB` or pass `--stdlib <directory>` to select a non-default standard-library directory. Keep the generated `.modules/` directory beside the executable because it contains the runtime-lazy module binaries.

## Backend boundary

The current runtime-lazy import and shared-module packaging path is implemented for the native x86-64 System V executable backend. The cross-target typed-IR backends do not yet lower runtime-lazy imports or bundle the Swan standard library. A successful import parse or semantic check does not imply that the selected target can link and run that module.

The standard library is intentionally small. General string values, file I/O, networking, and broad platform-specific runtime APIs are not currently provided.
