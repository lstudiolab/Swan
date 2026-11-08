# Swan Standard Library

The standard library is written in Swan and lives under `stdlib/`.

## Foundation

`foundation.swan` provides `echo_int`, `echo_bool`, `echo!(...)`, and
integer helpers including `abs_int`, `min_int`, `max_int`, `clamp_int`,
`sign_int`, `square_int`, `cube_int`, `power_int`, `factorial_int`,
`sum_to_int`, `average_int`, `is_even`, `is_odd`, and `in_range_int`.

`echo!("text")` is a compiler-recognized output intrinsic. It emits a direct
`puts` call for a string literal; it does not allocate a heap string or
format the value through `printf`. Integer and boolean output use `log`.

## Example

Until source-module imports are implemented, combine the standard-library
module and application source:

```sh
cat stdlib/foundation.swan main.swan > combined.swan
sh build.sh
./build/swan build combined.swan -o app
./app
```

Swan uses indentation-based blocks and does not use braces or an `end`
keyword. The compiler currently targets x86-64 System V Linux.

## Current limitations

Module resolution, floating-point execution, general strings, file I/O,
networking, and additional architecture backends are not yet implemented.
