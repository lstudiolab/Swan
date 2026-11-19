$ErrorActionPreference = "Stop"
if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
    throw "Brew currently targets Linux x86-64. Install WSL 2 with a Linux distribution, then run this installer again."
}
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$WslRoot = (& wsl wslpath -a "$Root").Trim()
if ($LASTEXITCODE -ne 0 -or -not $WslRoot) {
    throw "Could not resolve the Brew repository path inside WSL."
}
& wsl sh "$WslRoot/install.sh"
if ($LASTEXITCODE -ne 0) {
    throw "Brew installation failed inside WSL."
}
Write-Host "Brew installed inside WSL. Run it from your WSL terminal."
