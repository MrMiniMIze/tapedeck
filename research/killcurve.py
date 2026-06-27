"""Turn the eib_research returns CSV into a kill curve and honest diagnostics.

Usage:
    eib_research <itch-file> | python3 research/killcurve.py
    python3 research/killcurve.py returns.csv

The CSV has one column per fill model (conservative, optimistic). This reports
the Sharpe per model (the kill curve over fill aggressiveness), then treats the
per-model Sharpes as the trial ledger and reports the deflated Sharpe, a
bootstrap confidence interval, and a null test on the best model.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import honest_eval as he


def read_csv(stream):
    header = stream.readline().strip().split(",")
    cols = {h: [] for h in header}
    for line in stream:
        line = line.strip()
        if not line:
            continue
        for h, v in zip(header, line.split(",")):
            cols[h].append(float(v))
    return cols


def main():
    src = open(sys.argv[1]) if len(sys.argv) > 1 else sys.stdin
    cols = read_csv(src)
    cols = {k: v for k, v in cols.items() if v}
    if not cols:
        print("no return data on input")
        return

    curve = he.kill_curve(cols)
    print("Kill curve (per period Sharpe by fill model):")
    for k in cols:
        print(f"  {k:14s} sharpe={curve[k]: .4f}   periods={len(cols[k])}")

    best = max(cols, key=lambda k: curve[k])
    print(f"\nBest fill model: {best}")
    trials = [curve[k] for k in cols]
    if len(trials) >= 2:
        try:
            dsr = he.deflated_sharpe_ratio(cols[best], trials)
            print(f"  deflated Sharpe prob vs {len(trials)} fill-model trials: {dsr:.4f}")
        except Exception as exc:  # noqa: BLE001
            print(f"  deflated Sharpe: not available ({exc})")
    lo, hi = he.stationary_bootstrap_sharpe_ci(cols[best], n_boot=1000)
    print(f"  bootstrap 95% Sharpe CI: [{lo:.4f}, {hi:.4f}]")
    p_value, real, _ = he.null_shuffle_test(cols[best], n=1000)
    print(f"  null/shuffle p-value: {p_value:.4f}  (real sharpe {real:.4f})")

    print("\nNote: synthetic or short data is a null by design. Real numbers need")
    print("a real ITCH sample; latency claims need a tuned bare-metal host.")


if __name__ == "__main__":
    main()
