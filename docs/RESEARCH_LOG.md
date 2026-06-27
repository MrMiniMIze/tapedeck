# Research Log / Trial Ledger

> **Living document, update it every time you try anything.** Every
> configuration you test (signal variant, threshold, lookback, horizon, feature)
> is a *trial* and must be counted in the Deflated Sharpe Ratio. `N = 1` is a lie
> if you tuned anything; exploration during development *is* multiple testing.

| # | Date | What changed (signal / threshold / horizon / feature) | Sharpe (net of spread) | Kept? |
|---|------|-------------------------------------------------------|------------------------|-------|
| 1 | 2026-06-27 | Aggressive microprice, AAPL, hold 1 sample | -0.4597 | no |
| 2 | 2026-06-27 | Aggressive microprice, AAPL, hold 5 samples | -0.1191 | no |
| 3 | 2026-06-27 | Aggressive microprice, AAPL, hold 20 samples | -0.0537 | no |
| 4 | 2026-06-27 | Aggressive microprice, AAPL, hold 100 samples | 0.0000 | no |
| 5 | 2026-06-27 | Generalization: same strategy on MSFT, hold 1 | -0.1764 | no |
| 6 | 2026-06-27 | Generalization: same strategy on MSFT, hold 5 | -0.0344 | no |
| 7 | 2026-06-27 | Generalization: same strategy on MSFT, hold 20 | -0.0093 | no |
| 8 | 2026-06-27 | Generalization: same strategy on MSFT, hold 100 | 0.0002 | no |

## First real run (the null)

Data: NASDAQ TotalView-ITCH 5.0, 2019-12-30, first ~1.5 GB slice, symbol AAPL
(335,809 events). Strategy: take a +/-1 position in the microprice direction,
re-evaluated each holding horizon, filled aggressively at the touch so the spread
is a paid cost. Result: the kill curve over holding horizon is negative and worst
at the shortest horizon (most spread paid), decaying toward zero as turnover
falls. The best horizon (100) has a deflated-Sharpe probability of ~0.00, a
bootstrap 95% CI of [-0.029, +0.028] straddling zero, and a null p-value of 0.51.

**Conclusion: no edge net of the spread.** This is the honest null. The microprice
predicts the next move, but not by enough to cross the spread. The same null holds
on MSFT with no retuning (h1 -0.18 down to h100 ~0), so it is not an AAPL artifact.
Caveats: two symbols, one day, one slice, an aggressive-take strategy, and only
four horizon trials. A genuine study would add more days, the passive
(queue-position) variant, and report the deflated Sharpe over the full committed
trial count.

## Deflated Sharpe inputs (fill at Phase 3)

- Trial count **N** (= number of rows above): _TODO_
- Estimated correlation among trials: _TODO_
- Return skew / kurtosis: _TODO_
- **Raw Sharpe:** _TODO_  |  **Deflated Sharpe:** _TODO_
- Stationary-bootstrap 95% CI on Sharpe, lower bound: _TODO_ (excludes zero after deflation? _TODO_)
- Null/shuffle test, noise-Sharpe distribution & where the real Sharpe sits: _TODO_
- Generalization check (second symbol/window, **no retuning**): _TODO_
