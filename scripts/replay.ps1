# Build the replay tool and run it on a real ITCH 5.0 file.
# Usage: scripts\replay.ps1 <itch-5.0-file> [preset]
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

if ($args.Count -lt 1) { Write-Error "usage: scripts\replay.ps1 <itch-5.0-file> [preset]" }
$file = $args[0]
$preset = if ($args.Count -ge 2) { $args[1] } else { "dev" }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Error "cmake not found. Run scripts\setup.ps1 first."
}

cmake --preset $preset | Out-Null
cmake --build --preset $preset --target eib_replay
$bin = Get-ChildItem -Path "build\$preset" -Recurse -Filter eib_replay.exe | Select-Object -First 1
if (-not $bin) { Write-Error "eib_replay.exe not found after build" }
& $bin.FullName $file
