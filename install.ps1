$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root "build"
New-Item -ItemType Directory -Force -Path $Build | Out-Null

$Sources = Get-ChildItem (Join-Path $Root "compiler/src") -Recurse -Filter "*.cpp" |
    ForEach-Object { $_.FullName }

if (Get-Command clang++ -ErrorAction SilentlyContinue) {
    & clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic "-I$(Join-Path $Root 'compiler/include')" @Sources -o (Join-Path $Build "swan.exe")
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    & g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic "-I$(Join-Path $Root 'compiler/include')" @Sources -o (Join-Path $Build "swan.exe")
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    Push-Location $Root
    try {
        & cl /nologo /std:c++20 /O2 /EHsc /W4 "/I$(Join-Path $Root 'compiler/include')" @Sources "/Fe:$(Join-Path $Build 'swan.exe')"
    } finally {
        Pop-Location
    }
} else {
    throw "Install a C++20 compiler (Clang, GCC, or MSVC) first."
}
if ($LASTEXITCODE -ne 0) { throw "Swan build failed." }
Write-Host "Built $(Join-Path $Build 'swan.exe')"
