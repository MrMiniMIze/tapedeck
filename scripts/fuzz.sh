#!/usr/bin/env bash
# Build and run the ITCH parser fuzzer. Requires clang with libFuzzer (Linux;
# Apple clang does not ship the libFuzzer runtime).
# Usage: scripts/fuzz.sh [seconds]   (default 30)
set -euo pipefail
cd "$(dirname "$0")/.."

SECS="${1:-30}"
command -v clang++ >/dev/null 2>&1 || { echo "clang++ with libFuzzer is required"; exit 1; }

cmake -S . -B build/fuzz -DEIB_FUZZ=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fuzz --target fuzz_itch
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  build/fuzz/fuzz/fuzz_itch -max_total_time="$SECS" -print_final_stats=1
