# Exchange-in-a-Box (tapedeck)

End-to-end C++20 market-data pipeline on one **deterministic, replayable** spine:

> **feed parser, order-book reconstruction, event-driven backtester, one costed order-flow signal**

The point is not "I built an exchange." The point is a measurably correct, deterministic, honestly benchmarked pipeline, with a written account of exactly where it lies.

> **Status: Phases 0 to 3 complete and CI-green.** The full pipeline (ITCH 5.0 parser, flat `BookBuilder`, `MatchEngine` with queue-position fills, event-driven backtester with a lookahead oracle, OFI/microprice signal, and the deflated-Sharpe evaluation machine) has been run end to end on real NASDAQ data. The honest finding: the microprice signal does not survive the spread (see **Results** below). Remaining: Phase 4 (honest latency on bare metal), which is optional.

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

## Results: the honest null on real data

Run end to end on real **NASDAQ TotalView-ITCH 5.0** (2019-12-30 slice). The strategy takes a +/-1 position in the microprice direction, filled aggressively at the touch so the spread is a paid cost, swept over holding horizon. The kill curve is Sharpe (per period, net of the spread) versus horizon:

| Holding horizon | AAPL Sharpe | MSFT Sharpe |
|-----------------|-------------|-------------|
| 1               | -0.46       | -0.18       |
| 5               | -0.12       | -0.03       |
| 20              | -0.05       | -0.01       |
| 100             |  0.00       |  0.00       |

The signal does not survive the spread. The loss is largest at the shortest horizon (where you cross the spread most) and decays toward zero as turnover falls. On both names the best horizon has a deflated-Sharpe probability near zero, a bootstrap 95% Sharpe CI straddling zero, and a null-shuffle p-value near 0.5. The same null holds on MSFT with no retuning, so it is not an AAPL artifact.

That is the point of the project: a rigorous pipeline that finds the null and reports it, rather than a fitted Sharpe that would not survive scrutiny. Full trial ledger in `docs/RESEARCH_LOG.md`. Reproduce with `scripts/research <itch-file> AAPL`.

## Roadmap

* [x] **Phase 0**: scaffold (CMake, CI on Linux and Windows, Catch2, sanitizers, platform clock and affinity, honesty docs)
* [x] **Phase 1**: flat-array `BookBuilder` (differential-tested vs a reference book), ITCH 5.0 `FeedParser`, `eib_replay`, a libFuzzer parser target, deterministic `state_hash`, and a CI-enforced zero-allocation steady state
* [x] **Phase 2**: `MatchEngine` (queue-position fills, Conservative and Optimistic), the event-driven backtester (book plus matcher on one timestamp-ordered stream, no lookahead by construction), and the lookahead oracle test (foresight prints money, the honest path does not)
* [x] **Phase 3**: OFI and microprice signals (integer-only), the honest-evaluation machine (deflated Sharpe, bootstrap CI, null test, kill curves), and the end-to-end driver (`eib_research` plus `scripts/research`), run on real NASDAQ data: the microprice edge does not survive the spread on AAPL or MSFT (see Results)
* [ ] **Phase 4**: honest latency on bare-metal Linux, or a throughput plus determinism reframe

## Layout

```
include/eib/     headers: types, event, order_book, match_engine, backtester, signal, itch_parser, reference_book, hash, clock, affinity
src/             core static lib: order_book.cpp, itch_parser.cpp, match_engine.cpp
apps/            eib_replay (replay a real ITCH file), eib_research (kill curves), eib_zalloc_check
tests/           Catch2 tests (auto-fetched): order book, ITCH parser, match engine, backtester, signal, differential
fuzz/            libFuzzer harness for the ITCH parser (build with EIB_FUZZ=ON)
research/        honest-evaluation machine in Python (deflated Sharpe, bootstrap, null test) plus killcurve.py
scripts/         setup / build / test / replay / fuzz / research / selfcheck / bench / clean  (.sh + .ps1)
docs/            project docs: LATENCY, FILLS, RESEARCH_LOG, BENCHMARKS, DATA, DESIGN
.github/         CI on Linux and Windows: build+test, ASan/UBSan, zero-alloc, fuzz, research
```

## Honesty docs

* `docs/LATENCY.md`: measurement methodology and the environment rules
* `docs/FILLS.md`: fill-model assumptions and the sensitivity (kill) curves
* `docs/RESEARCH_LOG.md`: the trial ledger feeding the Deflated Sharpe Ratio
* `docs/BENCHMARKS.md`: environment-gated results
* `docs/DATA.md`: the ITCH 5.0 venue spine, where to get it, framing, message types
* `docs/DESIGN.md`: the load-bearing decisions, what CI proves, and where it lies
