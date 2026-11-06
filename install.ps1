$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = if ($env:SWAN_BUILD_DIR) { $env:SWAN_BUILD_DIR } else { Join-Path $Root "build" }
$Prefix = if ($env:SWAN_INSTALL_PREFIX) { $env:SWAN_INSTALL_PREFIX } else { Join-Path $HOME ".local" }

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "Swan requires CMake 3.20+. Install CMake and a C++17 toolchain, then run this script again."
}

Write-Host "Configuring Swan..."
cmake -S $Root -B $Build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

Write-Host "Building Swan..."
cmake --build $Build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Swan build failed." }

if ($env:SWAN_SKIP_TESTS -ne "1") {
    Write-Host "Running compiler tests..."
    ctest --test-dir $Build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Swan tests failed. Installation was stopped." }
}

Write-Host "Installing Swan to $Prefix..."
cmake --install $Build --config Release --prefix $Prefix
if ($LASTEXITCODE -ne 0) { throw "Swan installation failed." }

$Bin = Join-Path $Prefix "bin"
$Swan = Join-Path $Bin "swan.exe"
if (-not (Test-Path $Swan)) {
    $Swan = Join-Path $Bin "swan"
}
if (-not (Test-Path $Swan)) { throw "Installation finished, but the Swan executable was not found in $Bin." }

$UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
$PathParts = @($UserPath -split ";" | Where-Object { $_ })
if ($PathParts -notcontains $Bin) {
    $NewPath = (@($PathParts) + $Bin) -join ";"
    [Environment]::SetEnvironmentVariable("Path", $NewPath, "User")
}
$env:Path = "$Bin;$env:Path"

Write-Host ""
Write-Host "Swan is installed. Try: swan check path\to\file.swan"
Write-Host "Open a new terminal if Windows does not recognize the 'swan' command yet."
