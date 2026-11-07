# Swan Standard Library

The standard library is written in Swan wherever the current language can express the feature.

## Foundation

`foundation.swan` is the first standard-library module.

Current API:

- `echo(message: string) -> void`: writes the message followed by a newline.

Example:

```swan
fn main() -> int {
    echo("Hello from Swan");
    return 0;
}
```

## Loading status

The compiler parses `import` syntax, but module resolution and linking are not implemented yet. Until that compiler feature lands, combine a library source file with the program before compiling:

```sh
cat stdlib/foundation.swan main.swan > combined.swan
./build/swan combined.swan -o combined.c
cc -std=c11 combined.c -o combined
```

This is a temporary source-composition workflow, not the final import system. Do not treat `import foundation` as supported yet.
