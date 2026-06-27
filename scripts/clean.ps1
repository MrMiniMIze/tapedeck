# Remove build output.
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
if (Test-Path build) { Remove-Item -Recurse -Force build }
Write-Host "Removed build/."
