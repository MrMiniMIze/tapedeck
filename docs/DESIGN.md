# Design notes

What the load-bearing decisions are, why they were made, and exactly where this
project is honest about its limits. The selling point is not "I built an
exchange." It is a measurably correct, deterministic, honestly benchmarked
pipeline with a written account of where it lies.

## The spine

```
ITCH 5.0 feed -> BookBuilder -> MatchEngine -> event-driven backtester -> OFI/microprice signal -> honest-evaluation machine
```

One normalized event stream drives everything in timestamp order.

## Decisions that carry weight

**Determinism is the foundation, not a nicety.** Same input gives a bit
identical book and event log, captured by `state_hash` and enforced in CI. A
nondeterministic backtest is unfalsifiable: you cannot regression test the
engine, A/B a latency change, or trust a P&L number. Determinism is what makes
every downstream claim checkable.

**`BookBuilder` is not `MatchEngine`.** The feed already encodes the venue's own
matching decisions, so the BookBuilder only reconstructs the historical book from
L3 deltas. The MatchEngine does real work only for our own simulated orders. They
are separate types so the boundary is impossible to blur.

**The flat order book is validated by a simpler one.** The production book is a
flat tick-indexed array with a custom open-addressing id index (backward-shift
deletion) for O(1) updates and an allocation free steady state. A sorted
`std::map` reference book is kept as an obviously correct oracle, and a
differential test drives random histories through both and requires identical
state after every event. Fast and fiddly is trusted because simple and obvious
agrees with it.

**Zero allocation is a CI-enforced fact, not a claim.** A standalone guard
overrides global `operator new` to abort, then replays a workload through
`apply()`; if anything allocates on the steady-state path, the job fails.

**No lookahead by construction.** The backtester hands the strategy only state up
to the current event. The oracle test injects future information to show the
contrast: a perfect-foresight strategy prints money while the honest path loses
on the same tape. The gap is the value of the guarantee.

**Integers everywhere in the engine.** Prices are integer ticks, never floats.
Even the microprice signal stays integer: its direction reduces to an exact
cross-multiplication (whichever side rests more size), so the signal composes
with the determinism guarantees.

**The signal is a known quantity, deliberately.** OFI (Cont, Kukanov, Stoikov)
is heavily published; it is used here to exercise the honest-evaluation machine,
not presented as a discovered edge.

**Honesty is mechanized.** The evaluation layer computes the deflated Sharpe
ratio (which corrects for how many configurations were tried), a stationary
bootstrap confidence interval, and a sign-flip null test. Its self-test proves it
deflates the best of fifty pure-noise trials to non-significant. The kill curve
sweeps the fill model from Conservative to Optimistic; the crossover where the
edge dies is the headline. If the edge dies net of costs, the project leads with
the null.

## What CI actually proves

Build and tests on GCC, MSVC, and clang under `-Werror` / `/WX`; ASan and UBSan;
a libFuzzer pass over the binary parser; the zero-allocation guard; the
differential test under sanitizers; and the Python self-test of the evaluation
machine. An adversarial review pass also found and fixed a real latent UB (a
signed-overflow in the out-of-window check) that the green tests had missed.

## Where it lies (the honest limits)

* **One venue, one signal.** NASDAQ ITCH 5.0 only, OFI/microprice only. Depth
  over breadth by choice; multi-venue normalization is explicitly not attempted.
* **Queue position is a cumulative-volume model, not per-order FIFO.** Our order
  advances as volume executes through the queue ahead of it; it does not track
  each venue order's identity. A reasonable, common approximation, stated so it
  is not mistaken for more.
* **The fill model brackets reality, it does not nail it.** Conservative and
  Optimistic are the two ends; the truth is in between, which is why the kill
  curve sweeps it rather than asserting one number.
* **Latency is not claimed here.** Any nanosecond figure requires a tuned,
  bare-metal x86 Linux host. On macOS, Docker, WSL2, or a cloud VM the numbers
  are meaningless, so they are simply not reported. See `docs/LATENCY.md`.
* **The research numbers are pending real data.** The signal and the evaluation
  machine are built and self-tested on synthetic series; a real Sharpe and real
  kill curves require a real ITCH sample run on a tuned host. See `docs/DATA.md`.

## Not done yet

* End-to-end kill curves on a real ITCH sample (data dependent).
* Honest latency on bare-metal Linux, or the throughput-and-determinism reframe
  (Phase 4).
