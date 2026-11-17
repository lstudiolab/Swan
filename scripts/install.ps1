$ErrorActionPreference = "Stop"

$Repository = "lstudiolab/Swan"
$InstallRoot = Join-Path $env:LOCALAPPDATA "Swan"
$Bin = Join-Path $InstallRoot "bin"
$Zip = Join-Path $env:TEMP "swan-windows-x86_64.zip"
$Url = "https://github.com/$Repository/releases/latest/download/swan-windows-x86_64.zip"

New-Item -ItemType Directory -Force -Path $Bin | Out-Null
Invoke-WebRequest -Uri $Url -OutFile $Zip
$Extract = Join-Path $env:TEMP ("swan-" + [guid]::NewGuid().ToString())
Expand-Archive -Path $Zip -DestinationPath $Extract -Force
Copy-Item (Join-Path $Extract "swan/bin/*") $Bin -Force
Remove-Item $Extract -Recurse -Force
Remove-Item $Zip -Force

$UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
$Entries = @($UserPath -split ";" | Where-Object { $_ })
if ($Entries -notcontains $Bin) {
    [Environment]::SetEnvironmentVariable("Path", (($Entries + $Bin) -join ";"), "User")
}
Write-Host "Swan installed to $Bin"
Write-Host "Restart PowerShell, then run: swanc --version"
Write-Host "For native executable linking, install MSYS2 GCC and ensure its usr/bin directory is on PATH."
