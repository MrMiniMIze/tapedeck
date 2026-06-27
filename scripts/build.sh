#!/usr/bin/env bash
# Configure + build. Usage: scripts/build.sh [preset]   (preset: dev | asan)
set -euo pipefail
cd "$(dirname "$0")/.."

PRESET="${1:-dev}"
command -v cmake >/dev/null 2>&1 || { echo "cmake not found. Run scripts/setup.sh first."; exit 1; }

echo ">> configure (preset: $PRESET)"
cmake --preset "$PRESET"
echo ">> build (preset: $PRESET)"
cmake --build --preset "$PRESET"
echo ">> build OK (preset: $PRESET)"
