$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root "build"
$Main = Join-Path $Root "main.cpp"
$Include = Join-Path $Root "compiler/include"
New-Item -ItemType Directory -Force -Path $Build | Out-Null
$Output = Join-Path $Build "swan.exe"

if (Get-Command clang++ -ErrorAction SilentlyContinue) {
    & clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic "-I$Include" $Main -o $Output
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    & g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic "-I$Include" $Main -o $Output
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    Push-Location $Root
    try {
        & cl /nologo /std:c++20 /O2 /EHsc /W4 "/I$Include" $Main "/Fe:$Output"
    } finally {
        Pop-Location
    }
} else {
    throw "Install a C++20 compiler (Clang, GCC, or MSVC) first."
}
if ($LASTEXITCODE -ne 0) { throw "Swan build failed." }
Write-Host "Built $Output"
