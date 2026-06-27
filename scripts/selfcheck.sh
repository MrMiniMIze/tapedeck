#!/usr/bin/env bash
# Report OS/arch, virtualization, tools, and whether this host can produce
# trustworthy latency numbers (the project's hard rule).
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== Exchange-in-a-Box :: environment self-check =="

uname_s="$(uname -s 2>/dev/null || echo unknown)"
uname_m="$(uname -m 2>/dev/null || echo unknown)"
echo "OS:    $uname_s"
echo "Arch:  $uname_m"

case "$uname_m" in
  x86_64|amd64|i?86) is_x86=1 ;;
  *) is_x86=0 ;;
esac
echo "x86 (TSC-capable build):  $([ "$is_x86" = 1 ] && echo yes || echo NO)"

# --- virtualization / container / WSL detection (best effort) ---
virt="bare-metal-or-unknown"
[ -f /.dockerenv ] && virt="docker"
if grep -qiE "microsoft|wsl" /proc/version 2>/dev/null; then virt="wsl"; fi
[ -n "${WSL_DISTRO_NAME:-}" ] && virt="wsl"
if command -v systemd-detect-virt >/dev/null 2>&1; then
  d="$(systemd-detect-virt 2>/dev/null || true)"
  [ -n "$d" ] && [ "$d" != "none" ] && virt="$d"
fi
echo "Environment:  $virt"

# --- tools ---
for t in cmake ctest git; do
  if command -v "$t" >/dev/null 2>&1; then
    echo "$t:  $("$t" --version 2>/dev/null | head -1)"
  else
    echo "$t:  NOT INSTALLED"
  fi
done
command -v clang++ >/dev/null 2>&1 && echo "clang++:  $(clang++ --version | head -1)"
command -v g++     >/dev/null 2>&1 && echo "g++:      $(g++ --version | head -1)"

echo
# --- latency verdict ---
latency_ok=1
[ "$is_x86" = 1 ] || latency_ok=0
case "$virt" in docker|wsl|kvm|vmware|qemu|xen|oracle|amazon|google|microsoft) latency_ok=0 ;; esac

if [ "$latency_ok" = 1 ]; then
  echo "LATENCY VERDICT: candidate bare-metal x86 host."
  echo "  Still confirm before quoting numbers: invariant TSC (constant_tsc nonstop_tsc),"
  echo "  cores isolated (isolcpus/nohz_full), turbo + C-states off. See docs/LATENCY.md."
else
  echo "LATENCY VERDICT: NOT valid for nanosecond latency claims."
  echo "  Fine for dev / tests / correctness / determinism."
  echo "  For Phase 4 numbers: dual-boot your PC into tuned x86 Linux, or reframe to"
  echo "  throughput + determinism + correctness. See docs/LATENCY.md."
fi
