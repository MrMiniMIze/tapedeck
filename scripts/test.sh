#!/usr/bin/env bash
# Configure + build + run tests. Usage: scripts/test.sh [preset]  (preset: dev | asan)
set -euo pipefail
cd "$(dirname "$0")/.."

PRESET="${1:-dev}"
command -v cmake >/dev/null 2>&1 || { echo "cmake not found. Run scripts/setup.sh first."; exit 1; }

cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
ctest --preset "$PRESET"
