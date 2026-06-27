"""Self test for the honest evaluation machine.

It does not assert exact numbers (those depend on the RNG); it asserts the
qualitative behavior that makes the machine trustworthy, with wide margins:

  * a planted edge is detected (high deflated Sharpe, tiny null p value,
    bootstrap lower bound above zero);
  * pure noise searched over many trials is correctly NOT detected (the best of
    fifty noise Sharpes deflates to non significant), which is the multiple
    testing inoculation;
  * a single noise series is consistent with the null.

Run with: python3 -m unittest discover research   (no third party packages).
"""

import math
import random
import unittest

import honest_eval as he


def gauss_series(n, mean, sd, seed):
    rng = random.Random(seed)
    return [rng.gauss(mean, sd) for _ in range(n)]


class TestNormalHelpers(unittest.TestCase):
    def test_ppf_cdf_round_trip(self):
        for x in (-2.5, -1.0, -0.1, 0.3, 1.4, 2.7):
            self.assertAlmostEqual(he.normal_ppf(he.normal_cdf(x)), x, places=4)

    def test_cdf_known_points(self):
        self.assertAlmostEqual(he.normal_cdf(0.0), 0.5, places=10)
        self.assertAlmostEqual(he.normal_cdf(1.645), 0.95, places=3)


class TestSharpeAndDeflation(unittest.TestCase):
    def test_planted_edge_is_detected(self):
        edge = gauss_series(5000, mean=0.10, sd=1.0, seed=1)
        trials = [he.sharpe(edge)] + [
            he.sharpe(gauss_series(5000, 0.0, 1.0, seed=100 + i)) for i in range(4)
        ]
        self.assertGreater(he.sharpe(edge), 0.0)
        dsr = he.deflated_sharpe_ratio(edge, trials)
        self.assertGreater(dsr, 0.95, f"edge should survive deflation, dsr={dsr}")

    def test_best_of_many_noise_trials_is_deflated_away(self):
        # Fifty independent noise strategies; the best looks good by luck.
        series = [gauss_series(2000, 0.0, 1.0, seed=500 + i) for i in range(50)]
        trial_sharpes = [he.sharpe(s) for s in series]
        best = max(range(len(series)), key=lambda i: trial_sharpes[i])
        raw = trial_sharpes[best]
        self.assertGreater(raw, 0.0, "best of fifty noise has a positive raw Sharpe")
        dsr = he.deflated_sharpe_ratio(series[best], trial_sharpes)
        self.assertLess(dsr, 0.95,
                        f"best of fifty noise must NOT be significant, dsr={dsr}")


class TestNullAndBootstrap(unittest.TestCase):
    def test_null_shuffle_flags_edge_and_clears_noise(self):
        edge = gauss_series(3000, mean=0.07, sd=1.0, seed=7)
        p_edge, _, _ = he.null_shuffle_test(edge, n=1000)
        self.assertLess(p_edge, 0.01, f"edge should beat the sign flip null, p={p_edge}")

        noise = gauss_series(3000, mean=0.0, sd=1.0, seed=8)
        p_noise, _, _ = he.null_shuffle_test(noise, n=1000)
        self.assertGreater(p_noise, 0.05, f"noise should not beat the null, p={p_noise}")

    def test_bootstrap_ci_sign(self):
        edge = gauss_series(3000, mean=0.08, sd=1.0, seed=11)
        lo, hi = he.stationary_bootstrap_sharpe_ci(edge, n_boot=1000)
        self.assertGreater(lo, 0.0, f"edge CI lower bound should exceed zero, lo={lo}")
        self.assertGreater(hi, lo)

        noise = gauss_series(3000, mean=0.0, sd=1.0, seed=12)
        nlo, nhi = he.stationary_bootstrap_sharpe_ci(noise, n_boot=1000)
        self.assertLess(nlo, 0.0, f"noise CI should straddle zero, lo={nlo}")
        self.assertGreater(nhi, 0.0, f"noise CI should straddle zero, hi={nhi}")


class TestWalkForwardAndKillCurve(unittest.TestCase):
    def test_walk_forward_splits(self):
        splits = he.walk_forward_splits(n=100, train=40, test=20, step=20)
        self.assertEqual(splits[0], ((0, 40), (40, 60)))
        self.assertEqual(splits[1], ((20, 60), (60, 80)))
        for (tr, te) in splits:
            self.assertLessEqual(te[1], 100)
            self.assertEqual(tr[1], te[0])

    def test_kill_curve_orders_by_sharpe(self):
        curve = he.kill_curve({
            "optimistic": gauss_series(2000, 0.05, 1.0, seed=21),
            "conservative": gauss_series(2000, -0.02, 1.0, seed=22),
        })
        self.assertGreater(curve["optimistic"], curve["conservative"])


if __name__ == "__main__":
    unittest.main()
