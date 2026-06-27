#!/usr/bin/env bash
# Verify build prerequisites; print install help if anything is missing.
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== setup: checking prerequisites =="
missing=0

if command -v cmake >/dev/null 2>&1; then
  echo "  ok: cmake ($(cmake --version | head -1))"
else
  echo "  MISSING: cmake"; missing=1
fi

if command -v clang++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1; then
  echo "  ok: C++ compiler"
else
  echo "  MISSING: C++ compiler (clang++ or g++)"; missing=1
fi

if command -v git >/dev/null 2>&1; then
  echo "  ok: git"
else
  echo "  MISSING: git"; missing=1
fi

if [ "$missing" = 0 ]; then
  echo
  echo "All prerequisites present. Next: scripts/build.sh && scripts/test.sh"
  exit 0
fi

echo
echo "Install instructions:"
echo "  Ubuntu / WSL2:  sudo apt update && sudo apt install -y build-essential cmake git"
echo "  macOS:          xcode-select --install && brew install cmake"
echo "  Windows:        use scripts\\setup.ps1 (winget install Kitware.CMake + a C++ toolchain)"
exit 1
