$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Get-Command py -ErrorAction SilentlyContinue
if ($Python) {
    $PythonCommand = "py"
} else {
    $Python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $Python) { throw "Swan requires Python 3.10 or newer." }
    $PythonCommand = "python"
}
& $PythonCommand -c "import sys; raise SystemExit(0 if sys.version_info >= (3,10) else 1)"
if ($LASTEXITCODE -ne 0) { throw "Swan requires Python 3.10 or newer." }
$env:PYTHONPATH = Join-Path $Root "lib"
& $PythonCommand -m unittest discover -s (Join-Path $Root "tests") -v
if ($LASTEXITCODE -ne 0) { throw "Swan tests failed." }
& $PythonCommand -m pip install --user $Root
if ($LASTEXITCODE -ne 0) { throw "Swan installation failed." }
Write-Host "Swan is installed. Try: swan exampleshello.swan -o hello.c"
Write-Host "The transpiler emits C source only; it does not compile or run the C program."
