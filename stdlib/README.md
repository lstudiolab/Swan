# Swan Standard Library

The standard library is written in Swan and lives under `stdlib/`. Imports are checked during compilation, but module binaries are loaded only when an imported function is first called. Swan also excludes unused functions from each generated module binary.

## Foundation

Import Foundation directly with `import foundation`. It provides
`echo_int`, `echo_bool`, the `echo!(...)` output intrinsic, and integer
helpers including `abs_int`, `min_int`, `max_int`, `clamp_int`,
`sign_int`, `square_int`, `cube_int`, `power_int`, `factorial_int`,
`sum_to_int`, `average_int`, `is_even`, `is_odd`, and `in_range_int`.

`echo!("text")` emits a direct `puts` call for a string literal. It does not
allocate a heap string or format the value through `printf`.

```swan
import foundation

fn main():
    echo!("Hello from Swan")
    echo_int(abs_int(-42))
```

Build with `swan build main.swan -o app`. Set `SWAN_STDLIB` or pass
`--stdlib <directory>` to use a non-default standard-library directory.

The compiler currently targets x86-64 System V Linux. Floating-point execution,
general strings, file I/O, networking, and additional architecture backends
are not yet implemented.
