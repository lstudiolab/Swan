#!/usr/bin/env python3
"""Print Hello World one billion times in Swan, Goose, Rust and C."""
import os
import pathlib
import platform
import shutil
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SWAN = ROOT / "build" / "swan"
N = 1_000_000_000

SOURCES = {
    "Swan": f"""fn main() -> int:
    var i = 0
    while i < {N}
        echo!("Hello World")
        i = i + 1
    return 0
""",
    "Goose": f"""import std;
fn main() {{
    var i: i64 = 0;
    while i < {N} {{
        print("Hello World");
        i += 1;
    }}
}}
""",
    "Rust": f"""fn main() {{
    for _ in 0..{N} {{
        println!("Hello World");
    }}
}}
""",
    "C": f"""#include <stdio.h>
int main(void) {{
    for (unsigned long long i = 0; i < {N}ULL; ++i)
        puts("Hello World");
    return 0;
}}
""",
}


def timed_compile(command):
    start = time.perf_counter()
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(
            f"Compilation failed: {' '.join(map(str, command))}\n{result.stderr}"
        )
    return elapsed


def timed_run(binary):
    # Discard the 12 GB of expected output. The program still performs every
    # print call; this excludes terminal rendering and disk-storage speed.
    start = time.perf_counter()
    with open(os.devnull, "wb") as sink:
        result = subprocess.run([str(binary)], cwd=ROOT, stdout=sink, stderr=subprocess.PIPE)
    elapsed = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(f"{binary.name} failed: {result.stderr.decode(errors='replace')}")
    return elapsed


def main():
    tools = {
        "Goose": os.environ.get("GOOSE", "goose"),
        "Rust": shutil.which("rustc"),
        "C": shutil.which("cc"),
    }
    if not SWAN.is_file():
        raise SystemExit("Build Swan first: sh build.sh")
    if not all(tools.values()):
        raise SystemExit("Goose, rustc and cc are required")

    with tempfile.TemporaryDirectory(prefix="swan-hello-billion-") as td:
        work = pathlib.Path(td)
        extensions = {"Swan": "swan", "Goose": "goose", "Rust": "rs", "C": "c"}
        binaries = {}
        compile_times = {}

        for name, source in SOURCES.items():
            src = work / f"hello.{extensions[name]}"
            binary = work / name.lower()
            src.write_text(source, encoding="utf-8")
            if name == "Swan":
                command = [str(SWAN), "build", str(src), "-o", str(binary)]
            elif name == "Goose":
                command = [tools[name], "build", "-o", str(binary), str(src)]
            elif name == "Rust":
                command = [tools[name], "-C", "opt-level=3", "-C", "target-cpu=native",
                           str(src), "-o", str(binary)]
            else:
                command = [tools[name], "-O3", "-march=native", "-DNDEBUG",
                           str(src), "-o", str(binary)]
            compile_times[name] = timed_compile(command)
            binaries[name] = binary

        print("Hello World x 1,000,000,000 benchmark", flush=True)
        print("Each program calls its language's standard line-print operation exactly 1 billion times.", flush=True)
        print("Output is redirected to /dev/null (12,000,000,000 expected bytes); terminal and disk speed excluded.", flush=True)
        print(f"Host: {platform.platform()} / {platform.machine()}", flush=True)
        print(f"CPU: {platform.processor() or 'not reported'}", flush=True)
        print("Compilation and runtime are wall-clock seconds. One runtime sample per language.", flush=True)
        print("", flush=True)
        print("| Language | Compile (s) | Runtime (s) |", flush=True)
        print("|---|---:|---:|", flush=True)

        # Rotate order to reduce systematic first/last-run bias.
        order = ["Swan", "Goose", "Rust", "C"]
        for name in order:
            runtime = timed_run(binaries[name])
            print(f"| {name} | {compile_times[name]:.6f} | {runtime:.6f} |", flush=True)


if __name__ == "__main__":
    main()
