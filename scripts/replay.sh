#!/usr/bin/env bash
# Build the replay tool and run it on a real ITCH 5.0 file.
# Usage: scripts/replay.sh <itch-5.0-file> [preset]
set -euo pipefail
cd "$(dirname "$0")/.."

[ "$#" -ge 1 ] || { echo "usage: scripts/replay.sh <itch-5.0-file> [preset]"; exit 2; }
FILE="$1"
PRESET="${2:-dev}"
command -v cmake >/dev/null 2>&1 || { echo "cmake not found. Run scripts/setup.sh first."; exit 1; }

cmake --preset "$PRESET" >/dev/null
cmake --build --preset "$PRESET" --target eib_replay
BIN="$(find "build/$PRESET" -name eib_replay -type f -perm -u+x 2>/dev/null | head -1)"
[ -n "$BIN" ] || { echo "eib_replay not found after build"; exit 1; }
"$BIN" "$FILE"
