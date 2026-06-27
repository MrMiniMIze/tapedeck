# Verify build prerequisites on Windows; print install help if anything is missing.
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

Write-Host "== setup: checking prerequisites =="
$missing = $false

if (Get-Command cmake -ErrorAction SilentlyContinue) {
  Write-Host "  ok: cmake ($((cmake --version | Select-Object -First 1)))"
} else { Write-Host "  MISSING: cmake"; $missing = $true }

if ((Get-Command cl -ErrorAction SilentlyContinue) -or (Get-Command clang++ -ErrorAction SilentlyContinue)) {
  Write-Host "  ok: C++ compiler"
} else { Write-Host "  MISSING: C++ compiler (MSVC 'cl' or clang++)"; $missing = $true }

if (Get-Command git -ErrorAction SilentlyContinue) {
  Write-Host "  ok: git"
} else { Write-Host "  MISSING: git"; $missing = $true }

if (-not $missing) {
  Write-Host ""
  Write-Host "All prerequisites present. Next: scripts\build.ps1 ; scripts\test.ps1"
  exit 0
}

Write-Host ""
Write-Host "Install on Windows:"
Write-Host "  winget install Kitware.CMake"
Write-Host "  winget install Microsoft.VisualStudio.2022.BuildTools   (check 'Desktop development with C++')"
Write-Host "    -- or --   winget install LLVM.LLVM"
Write-Host "  winget install Git.Git"
Write-Host ""
Write-Host "Tip: for the MSVC compiler, run these scripts from a 'Developer PowerShell for VS 2022',"
Write-Host "     or let CMake's Visual Studio generator find it automatically."
exit 1
