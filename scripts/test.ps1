# Configure + build + run tests. Usage: scripts\test.ps1 [preset]  (preset: dev | asan)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$preset = if ($args.Count -ge 1) { $args[0] } else { "dev" }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Error "cmake not found. Run scripts\setup.ps1 first."
}

cmake --preset $preset
cmake --build --preset $preset
ctest --preset $preset
