# Phase 4 latency benchmark (stub today). Runs the environment gate so the habit
# is in place from the start; must never emit latency numbers off bare metal.
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

Write-Host "== bench (Phase 4) =="
& (Join-Path $PSScriptRoot "selfcheck.ps1")
Write-Host ""
Write-Host "NOTE: the benchmark target does not exist yet (it arrives in Phase 4)."
Write-Host "When it does, this script will:"
Write-Host "  1. refuse to print latency percentiles unless selfcheck says bare-metal x86;"
Write-Host "  2. measure the two named intervals under the real replayed event mix;"
Write-Host "  3. write results into docs\BENCHMARKS.md and docs\LATENCY.md."
Write-Host "See docs/LATENCY.md for the methodology and the no-VM rule."
