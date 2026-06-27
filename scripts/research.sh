#!/usr/bin/env bash
# Build the research driver, run it on an ITCH file (or a synthetic stream), and
# print the kill curve and honest diagnostics.
# Usage: scripts/research.sh [itch-5.0-file]   (no file: synthetic plumbing run)
set -euo pipefail
cd "$(dirname "$0")/.."

INPUT="${1:---synth}"
PRESET="${2:-dev}"
command -v cmake >/dev/null 2>&1 || { echo "cmake not found. Run scripts/setup.sh first."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 required for the report"; exit 1; }

cmake --preset "$PRESET" >/dev/null
cmake --build --preset "$PRESET" --target eib_research
BIN="$(find "build/$PRESET" -name eib_research -type f -perm -u+x 2>/dev/null | head -1)"
[ -n "$BIN" ] || { echo "eib_research not found after build"; exit 1; }

"$BIN" "$INPUT" | python3 research/killcurve.py
