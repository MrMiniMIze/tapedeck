# Report OS/arch, virtualization, tools, and whether this host can produce
# trustworthy latency numbers (the project's hard rule).
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

Write-Host "== Exchange-in-a-Box :: environment self-check =="

$arch = $env:PROCESSOR_ARCHITECTURE
Write-Host "OS:    Windows"
Write-Host "Arch:  $arch"
$isX86 = ($arch -match "AMD64|x86|EM64T")
Write-Host ("x86 (TSC-capable build):  " + $(if ($isX86) { "yes" } else { "NO" }))

# VM detection (best effort)
$model = ""
try { $model = (Get-CimInstance Win32_ComputerSystem -ErrorAction Stop).Model } catch {}
$virt = "bare-metal-or-unknown"
if ($model -match "Virtual|VMware|KVM|Hyper-V|VirtualBox|Google|Amazon|QEMU") { $virt = "virtual ($model)" }
Write-Host "Environment:  $virt"

foreach ($t in @("cmake", "ctest", "git")) {
  $c = Get-Command $t -ErrorAction SilentlyContinue
  if ($c) { Write-Host ("{0}:  {1}" -f $t, ((& $t --version) | Select-Object -First 1)) }
  else { Write-Host ("{0}:  NOT INSTALLED" -f $t) }
}

Write-Host ""
$latencyOk = $isX86 -and ($virt -eq "bare-metal-or-unknown")
if ($latencyOk) {
  Write-Host "LATENCY VERDICT: x86 host, but Windows is NOT ideal for nanosecond latency."
  Write-Host "  Windows cannot isolate a core from the kernel/DPCs (no isolcpus/nohz_full),"
  Write-Host "  so the p99.9 tail carries OS jitter. For headline numbers, dual-boot this PC"
  Write-Host "  into tuned x86 Linux. See docs/LATENCY.md."
} else {
  Write-Host "LATENCY VERDICT: NOT valid for nanosecond latency claims."
  Write-Host "  Fine for dev / tests / correctness. For Phase 4 numbers use bare-metal x86 Linux."
  Write-Host "  See docs/LATENCY.md."
}
