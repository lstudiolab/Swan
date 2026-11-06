$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root "build"
New-Item -ItemType Directory -Force -Path $Build | Out-Null
if (Get-Command clang++ -ErrorAction SilentlyContinue) {
    $Compiler = "clang++"
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    $Compiler = "g++"
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    Push-Location $Root
    try { cl /nologo /std:c++20 /O2 /EHsc /W4 /Fe:"$Build\swan.exe" lib\swan.cpp }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Swan build failed." }
    Write-Host "Built $Build\swan.exe"
    exit 0
} else {
    throw "Install a C++20 compiler (Clang, GCC, or MSVC) first."
}
& $Compiler -std=c++20 -O2 -Wall -Wextra -Wpedantic (Join-Path $Root "lib/swan.cpp") -o (Join-Path $Build "swan.exe")
if ($LASTEXITCODE -ne 0) { throw "Swan build failed." }
Write-Host "Built $Build\swan.exe"
