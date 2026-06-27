# Exchange-in-a-Box (tapedeck)

End-to-end C++20 market-data pipeline on one **deterministic, replayable** spine:

> **feed parser, order-book reconstruction, event-driven backtester, one costed order-flow signal**

The point is not "I built an exchange." The point is a measurably correct, deterministic, honestly benchmarked pipeline, with a written account of exactly where it lies.

> **Status: Phase 2 complete.** On top of the Phase 1 pipeline (ITCH 5.0 parser, flat `BookBuilder`, fuzzing, zero-allocation steady state), the `MatchEngine` models our own orders' queue position and fills, the event-driven backtester runs the book and matcher on one timestamp-ordered stream with no lookahead by construction, and the lookahead oracle test shows foresight printing money while the honest path does not. Phase 3 is underway: the OFI and microprice signals and the honest-evaluation machine (deflated Sharpe, bootstrap CI, null test, kill curves) are built and self-tested; the end-to-end kill curves on real ITCH data are the remaining step.

## Quick start

Everything is wrapped in `scripts/`, so you never need to remember a `cmake` command. Pick your shell.

**Windows (PowerShell):**
```powershell
scripts\setup.ps1      # check prerequisites (and how to install them)
scripts\build.ps1      # configure + build
scripts\test.ps1       # build + run tests
scripts\selfcheck.ps1  # environment + latency-validity report
```

**Linux, WSL2, or macOS (bash):**
```bash
scripts/setup.sh
scripts/build.sh
scripts/test.sh
scripts/selfcheck.sh
```

## Prerequisites

Needs **CMake 3.20 or newer** and a **C++20** compiler (GCC 11+, Clang 14+, or MSVC 2022). Catch2 is fetched automatically by CMake, so there is nothing to install for tests.

| Platform | Install |
|----------|---------|
| **Windows** | `winget install Kitware.CMake`, then `winget install Microsoft.VisualStudio.2022.BuildTools` (select **Desktop development with C++**) or `winget install LLVM.LLVM`, plus `winget install Git.Git`. Tip: run the scripts from a *Developer PowerShell for VS 2022*. |
| **Ubuntu or WSL2** | `sudo apt update && sudo apt install -y build-essential cmake git` |
| **macOS** | `xcode-select --install && brew install cmake` |

`scripts/setup` checks all of this for you and prints the right command if something is missing.

## Scripts

| Script (`.sh` and `.ps1`) | What it does |
|---------------------------|--------------|
| `setup` | Verify cmake, compiler, and git; print install help if missing |
| `build [preset]` | `cmake --preset` then build. preset is `dev` (default) or `asan` |
| `test [preset]` | build then `ctest`. `test asan` runs ASan and UBSan (Linux, WSL2, macOS) |
| `replay <file>` | build then run the ITCH 5.0 replay tool on a real data file (see `docs/DATA.md`) |
| `fuzz [secs]` | build and run the ITCH parser fuzzer (clang plus libFuzzer, Linux); bash only |
| `research [file]` | build the research driver, run the signal per fill model, print the kill curve (no file: synthetic plumbing run) |
| `selfcheck` | OS and arch, VM/Docker/WSL detection, tool versions, and the **latency-validity verdict** |
| `bench` | Phase 4 latency benchmark (stub today); environment gated so it will not emit numbers off bare metal |
| `clean` | remove `build/` |

## Where to build versus where to measure

| Activity | Allowed environments |
|----------|----------------------|
| Dev, tests, determinism, fuzzing, correctness | anywhere: macOS, Linux, Windows, WSL2; x86 or arm64 |
| **Reported latency percentiles** | **only tuned, bare-metal x86 Linux**; never Docker, WSL2, or any cloud VM |

`scripts/selfcheck` tells you which bucket the current machine is in. For the Phase 4 latency numbers, the recommended path is dual-booting your own PC into a tuned Linux: free, real bare metal you control, strictly better than a rented cloud VM (which still virtualizes the clock and adds vCPU steal time). Details and the full methodology live in `docs/LATENCY.md`.

## Design commitments (what makes it credible)

* **Single deterministic, single-threaded hot path.** Determinism over raw throughput. Same input gives a bit-identical book and event log (verified in CI from Phase 1). A nondeterministic backtest is an unfalsifiable backtest.
* **`BookBuilder` is not `MatchEngine`.** Reconstructing the venue's book from L3/MBO deltas is a different job from matching your own simulated orders against it.
* **No lookahead by construction.** The backtester replays the same normalized event stream the live path would, in timestamp order. Proven by a lookahead oracle test (Phase 2).
* **Latency is measured honestly or not claimed at all.** See `docs/LATENCY.md`.

## Roadmap

* [x] **Phase 0**: scaffold (CMake, CI on Linux and Windows, Catch2, sanitizers, platform clock and affinity, honesty docs)
* [x] **Phase 1**: flat-array `BookBuilder` (differential-tested vs a reference book), ITCH 5.0 `FeedParser`, `eib_replay`, a libFuzzer parser target, deterministic `state_hash`, and a CI-enforced zero-allocation steady state
* [x] **Phase 2**: `MatchEngine` (queue-position fills, Conservative and Optimistic), the event-driven backtester (book plus matcher on one timestamp-ordered stream, no lookahead by construction), and the lookahead oracle test (foresight prints money, the honest path does not)
* [~] **Phase 3**: OFI and microprice signals (integer-only), the honest-evaluation machine (deflated Sharpe, bootstrap CI, null test, kill curves, self-tested in `research/`), and the end-to-end driver (`eib_research` plus `scripts/research`: signal per fill model, returns CSV, kill curve report) are all built and verified on synthetic data; the only remaining step is one command on a real ITCH sample
* [ ] **Phase 4**: honest latency on bare-metal Linux, or a throughput plus determinism reframe

## Layout

```
include/eib/     headers: types, event, order_book, itch_parser, clock, affinity
src/             core static lib: order_book.cpp, itch_parser.cpp
apps/            eib_replay: run the pipeline on a real ITCH file
tests/           Catch2 tests (auto-fetched): smoke, order_book, itch_parser
fuzz/            libFuzzer harness for the ITCH parser (build with EIB_FUZZ=ON)
scripts/         setup / build / test / replay / selfcheck / bench / clean  (.sh + .ps1)
docs/            project docs: LATENCY, FILLS, RESEARCH_LOG, BENCHMARKS, DATA
.github/         CI: build and test on Linux and Windows, plus ASan/UBSan and fuzz jobs
```

## Honesty docs

* `docs/LATENCY.md`: measurement methodology and the environment rules
* `docs/FILLS.md`: fill-model assumptions and the sensitivity (kill) curves
* `docs/RESEARCH_LOG.md`: the trial ledger feeding the Deflated Sharpe Ratio
* `docs/BENCHMARKS.md`: environment-gated results
* `docs/DATA.md`: the ITCH 5.0 venue spine, where to get it, framing, message types
* `docs/DESIGN.md`: the load-bearing decisions, what CI proves, and where it lies
