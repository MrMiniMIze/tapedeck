"""Honest evaluation of a trading return series.

Pure standard library, no third party dependencies, so the CI job is a single
python3 invocation and every result is reproducible with a fixed seed. The point
of this module is to make a backtested edge hard to fool yourself with:

  * sharpe                       per observation Sharpe ratio
  * deflated_sharpe_ratio        Bailey and Lopez de Prado, corrects for the
                                 number of trials (best of N noise looks good)
  * stationary_bootstrap_sharpe_ci   confidence interval that respects serial
                                 dependence (Politis and Romano)
  * null_shuffle_test            sign flip null, the real Sharpe must sit in the
                                 tail of the noise distribution
  * walk_forward_splits          train then test index windows
  * kill_curve                   Sharpe as a function of a swept parameter, e.g.
                                 the fill model from conservative to optimistic

The C++ backtest produces the aggregate return series; this layer evaluates it.
The numbers are real only on real data run on a tuned host; see docs/.
"""

from __future__ import annotations

import math
import random
from typing import Callable, Dict, List, Sequence, Tuple

_EULER_MASCHERONI = 0.5772156649015329


# --------------------------------------------------------------------------- #
# Moments and normal distribution helpers.
# --------------------------------------------------------------------------- #
def _mean(xs: Sequence[float]) -> float:
    return sum(xs) / len(xs)


def _variance_pop(xs: Sequence[float]) -> float:
    m = _mean(xs)
    return sum((x - m) ** 2 for x in xs) / len(xs)


def _variance_sample(xs: Sequence[float]) -> float:
    m = _mean(xs)
    return sum((x - m) ** 2 for x in xs) / (len(xs) - 1)


def _std_pop(xs: Sequence[float]) -> float:
    return math.sqrt(_variance_pop(xs))


def skewness(xs: Sequence[float]) -> float:
    """Population skewness."""
    s = _std_pop(xs)
    if s == 0.0:
        return 0.0
    m = _mean(xs)
    return sum((x - m) ** 3 for x in xs) / len(xs) / s ** 3


def kurtosis(xs: Sequence[float]) -> float:
    """Population kurtosis, NOT excess (a normal distribution gives 3)."""
    s = _std_pop(xs)
    if s == 0.0:
        return 3.0
    m = _mean(xs)
    return sum((x - m) ** 4 for x in xs) / len(xs) / s ** 4


def normal_cdf(x: float) -> float:
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def normal_ppf(p: float) -> float:
    """Inverse standard normal CDF via Acklam's rational approximation."""
    if not 0.0 < p < 1.0:
        raise ValueError("normal_ppf requires 0 < p < 1")
    a = [-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
         1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00]
    b = [-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
         6.680131188771972e+01, -1.328068155288572e+01]
    c = [-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
         -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00]
    d = [7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
         3.754408661907416e+00]
    plow, phigh = 0.02425, 1.0 - 0.02425
    if p < plow:
        q = math.sqrt(-2.0 * math.log(p))
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0)
    if p > phigh:
        q = math.sqrt(-2.0 * math.log(1.0 - p))
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0)
    q = p - 0.5
    r = q * q
    return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / \
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0)


# --------------------------------------------------------------------------- #
# Sharpe and the deflated Sharpe ratio.
# --------------------------------------------------------------------------- #
def sharpe(returns: Sequence[float]) -> float:
    """Per observation Sharpe. Annualize by multiplying by sqrt(periods/year)."""
    if len(returns) < 2:
        return 0.0
    s = _std_pop(returns)
    return 0.0 if s == 0.0 else _mean(returns) / s


def expected_max_sharpe(trial_sharpes: Sequence[float]) -> float:
    """Expected maximum per observation Sharpe under the null of N noise trials."""
    n = len(trial_sharpes)
    if n < 2:
        raise ValueError("deflation needs at least 2 trials")
    var_trials = _variance_sample(trial_sharpes)
    return math.sqrt(var_trials) * (
        (1.0 - _EULER_MASCHERONI) * normal_ppf(1.0 - 1.0 / n)
        + _EULER_MASCHERONI * normal_ppf(1.0 - 1.0 / (n * math.e))
    )


def deflated_sharpe_ratio(returns: Sequence[float],
                          trial_sharpes: Sequence[float]) -> float:
    """Probability the selected strategy's true Sharpe exceeds what N noise
    trials would produce. Pass the per observation Sharpe of EVERY configuration
    tried (the RESEARCH_LOG ledger); the selected strategy is the best of them.
    A value near 1 is significant after correcting for the search; near 0.5 or
    below means the result is consistent with luck across N trials.
    """
    t = len(returns)
    if t < 2:
        return 0.0
    sr_hat = max(trial_sharpes)
    sr0 = expected_max_sharpe(trial_sharpes)
    g3 = skewness(returns)
    g4 = kurtosis(returns)
    denom = math.sqrt(max(1e-12, 1.0 - g3 * sr_hat + (g4 - 1.0) / 4.0 * sr_hat ** 2))
    return normal_cdf((sr_hat - sr0) * math.sqrt(t - 1) / denom)


# --------------------------------------------------------------------------- #
# Resampling diagnostics.
# --------------------------------------------------------------------------- #
def stationary_bootstrap_sharpe_ci(returns: Sequence[float],
                                   n_boot: int = 2000,
                                   avg_block: float = 20.0,
                                   alpha: float = 0.05,
                                   seed: int = 12345) -> Tuple[float, float]:
    """Confidence interval on the Sharpe via the stationary bootstrap, which
    preserves serial dependence (autocorrelated returns break the naive CI)."""
    rng = random.Random(seed)
    t = len(returns)
    if t < 2:
        return (0.0, 0.0)
    p_jump = 1.0 / avg_block
    stats: List[float] = []
    for _ in range(n_boot):
        sample: List[float] = []
        idx = rng.randrange(t)
        for _ in range(t):
            sample.append(returns[idx])
            if rng.random() < p_jump:
                idx = rng.randrange(t)
            else:
                idx = (idx + 1) % t
        stats.append(sharpe(sample))
    stats.sort()
    lo = stats[int(alpha / 2.0 * n_boot)]
    hi = stats[min(n_boot - 1, int((1.0 - alpha / 2.0) * n_boot))]
    return (lo, hi)


def null_shuffle_test(returns: Sequence[float],
                      n: int = 2000,
                      seed: int = 6789) -> Tuple[float, float, List[float]]:
    """Sign flip null: randomly flip the sign of each return, which destroys any
    directional edge while keeping the magnitude distribution. Returns
    (p_value, real_sharpe, null_sharpes) where p_value is the fraction of null
    Sharpes at least as large as the real one (add one smoothed)."""
    rng = random.Random(seed)
    real = sharpe(returns)
    nulls: List[float] = []
    for _ in range(n):
        flipped = [r if rng.random() < 0.5 else -r for r in returns]
        nulls.append(sharpe(flipped))
    at_least = sum(1 for s in nulls if s >= real)
    p_value = (at_least + 1) / (n + 1)
    return (p_value, real, nulls)


# --------------------------------------------------------------------------- #
# Walk forward and the kill curve.
# --------------------------------------------------------------------------- #
def walk_forward_splits(n: int, train: int, test: int,
                        step: int | None = None
                        ) -> List[Tuple[Tuple[int, int], Tuple[int, int]]]:
    """Anchored sliding train then test index windows over [0, n)."""
    if step is None:
        step = test
    out: List[Tuple[Tuple[int, int], Tuple[int, int]]] = []
    start = 0
    while start + train + test <= n:
        out.append(((start, start + train),
                    (start + train, start + train + test)))
        start += step
    return out


def kill_curve(param_returns: Dict[object, Sequence[float]]
               ) -> Dict[object, float]:
    """Sharpe at each swept parameter value, e.g. fill model conservative to
    optimistic, or holding horizon. The C++ backtest supplies the return series
    per parameter; the crossover where Sharpe hits zero is the honest headline."""
    return {k: sharpe(v) for k, v in param_returns.items()}
