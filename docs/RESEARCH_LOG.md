# Research Log / Trial Ledger

> **Living document, update it every time you try anything.** Every
> configuration you test (signal variant, threshold, lookback, horizon, feature)
> is a *trial* and must be counted in the Deflated Sharpe Ratio. `N = 1` is a lie
> if you tuned anything; exploration during development *is* multiple testing.

| # | Date | What changed (signal / threshold / horizon / feature) | In-sample | Out-of-sample | Kept? |
|---|------|-------------------------------------------------------|-----------|---------------|-------|
| 1 |      |                                                       |           |               |       |

## Deflated Sharpe inputs (fill at Phase 3)

- Trial count **N** (= number of rows above): _TODO_
- Estimated correlation among trials: _TODO_
- Return skew / kurtosis: _TODO_
- **Raw Sharpe:** _TODO_  |  **Deflated Sharpe:** _TODO_
- Stationary-bootstrap 95% CI on Sharpe, lower bound: _TODO_ (excludes zero after deflation? _TODO_)
- Null/shuffle test, noise-Sharpe distribution & where the real Sharpe sits: _TODO_
- Generalization check (second symbol/window, **no retuning**): _TODO_
