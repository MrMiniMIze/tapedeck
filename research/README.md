# research: the honest-evaluation machine

Pure standard-library Python (no third party packages), so the CI job is a single
`python3` and every result is reproducible with a fixed seed.

The C++ engine produces an aggregate return series from a backtest; this layer
judges whether that series is a real edge or a story you told yourself.

## What is in `honest_eval.py`

| Function | What it guards against |
|----------|------------------------|
| `sharpe` | the headline number (per observation; annualize with `sqrt(periods/year)`) |
| `deflated_sharpe_ratio` | multiple testing: the best of N tried configs looks good by luck (Bailey and Lopez de Prado) |
| `stationary_bootstrap_sharpe_ci` | autocorrelated returns making the naive Sharpe error wrong (Politis and Romano) |
| `null_shuffle_test` | a Sharpe that a sign-flipped (no-edge) series could also produce |
| `walk_forward_splits` | fitting and reporting on the same data |
| `kill_curve` | a result that only survives under one optimistic assumption |

## How to use it with real results

1. Run the C++ backtest of the OFI/microprice strategy once per fill model
   (Conservative to Optimistic) and per holding horizon, writing the per period
   return series for each.
2. Pass every configuration's per observation Sharpe to `deflated_sharpe_ratio`
   as the trial ledger (this is the `N` that matters; see `docs/RESEARCH_LOG.md`).
3. Build the kill curves: `kill_curve({fill_model: returns, ...})` and the same
   over holding horizon and assumed latency. The crossover where Sharpe hits zero
   is the honest headline.
4. If the edge dies net of costs, lead with the null. A rigorously defended null
   beats a fitted Sharpe.

## Self test

`python3 -m unittest discover` (run from this directory) proves the machine
detects a planted edge and, crucially, that the best of fifty pure-noise trials
deflates to non-significant. The real numbers are valid only on real ITCH data
run on a tuned host; see `docs/LATENCY.md` and `docs/DATA.md`.
