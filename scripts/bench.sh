#!/usr/bin/env bash
# Phase 4 latency benchmark. Today: a stub that runs the environment gate so the
# habit is in place from the start. It must never emit latency numbers off bare metal.
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== bench (Phase 4) =="
./scripts/selfcheck.sh || true
echo
echo "NOTE: the benchmark target does not exist yet (it arrives in Phase 4)."
echo "When it does, this script will:"
echo "  1. refuse to print latency percentiles unless selfcheck says bare-metal x86;"
echo "  2. measure the two named intervals under the real replayed event mix;"
echo "  3. write results into docs/BENCHMARKS.md and docs/LATENCY.md."
echo "See docs/LATENCY.md for the methodology and the no-VM rule."
