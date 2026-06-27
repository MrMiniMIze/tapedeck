# Build the research driver, run it on an ITCH file (or a synthetic stream), and
# print the kill curve and honest diagnostics.
# Usage: scripts\research.ps1 [itch-5.0-file]   (no file: synthetic plumbing run)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$input_arg = if ($args.Count -ge 1) { $args[0] } else { "--synth" }
$preset = if ($args.Count -ge 2) { $args[1] } else { "dev" }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Write-Error "cmake not found. Run scripts\setup.ps1 first." }
if (-not (Get-Command python3 -ErrorAction SilentlyContinue)) { Write-Error "python3 required for the report" }

cmake --preset $preset | Out-Null
cmake --build --preset $preset --target eib_research
$bin = Get-ChildItem -Path "build\$preset" -Recurse -Filter eib_research.exe | Select-Object -First 1
if (-not $bin) { Write-Error "eib_research.exe not found after build" }

& $bin.FullName $input_arg | python3 research/killcurve.py
