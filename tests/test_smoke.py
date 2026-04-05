import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = REPO_ROOT / "build"
BINARY = BUILD_DIR / "cpp_backtester"
TIMEOUT_SECONDS = 60


class SmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(
            ["cmake", "-S", str(REPO_ROOT), "-B", str(BUILD_DIR)],
            check=True,
            timeout=TIMEOUT_SECONDS,
        )
        subprocess.run(
            ["cmake", "--build", str(BUILD_DIR), "-j2"],
            check=True,
            timeout=TIMEOUT_SECONDS,
        )
        if not BINARY.exists():
            raise AssertionError(f"Missing compiled binary: {BINARY}")

    def run_command(self, command: str, config_name: str):
        out_dir = Path(tempfile.mkdtemp(prefix=f"cpp_backtester_{command}_"))
        config_path = REPO_ROOT / "config" / config_name
        try:
            subprocess.run(
                [str(BINARY), command, str(config_path), str(out_dir)],
                check=True,
                timeout=TIMEOUT_SECONDS,
            )
            return out_dir
        except Exception:
            shutil.rmtree(out_dir, ignore_errors=True)
            raise

    def test_generate_prices_smoke(self):
        out_dir = self.run_command("generate", "smoke_simulation.json")
        try:
            prices = out_dir / "prices.csv"
            self.assertTrue(prices.exists(), "prices.csv should be created")
            lines = prices.read_text().strip().splitlines()
            self.assertGreater(len(lines), 10)
            self.assertEqual(lines[0], "day,open,high,low,close,atr,jump_applied")
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)

    def test_simulation_outputs_smoke(self):
        out_dir = self.run_command("simulate", "smoke_simulation.json")
        try:
            summary = json.loads((out_dir / "summary.json").read_text())
            self.assertTrue((out_dir / "trade_log.csv").exists())
            self.assertTrue((out_dir / "equity.csv").exists())
            self.assertTrue(summary["used_atr_thresholds"])
            self.assertTrue(summary["used_jump_diffusion"])
            self.assertGreater(summary["buy_count"] + summary["sell_count"], 0)
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)

    def test_json_policy_switches_smoke(self):
        out_dir = self.run_command("simulate", "smoke_policy_switches.json")
        try:
            summary = json.loads((out_dir / "summary.json").read_text())
            self.assertEqual(summary["buy_sizing_mode"], "fixed")
            self.assertEqual(summary["fully_utilized_mode"], "rebalance_placeholder")
            self.assertEqual(summary["gap_handling_mode"], "catch_up")
            self.assertTrue(summary["allow_sell_at_loss"])
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)

    def test_monte_carlo_500_runs_smoke(self):
        out_dir = self.run_command("monte-carlo", "smoke_monte_carlo.json")
        try:
            aggregate = json.loads((out_dir / "aggregate.json").read_text())
            self.assertEqual(aggregate["num_simulations_requested"], 500)
            self.assertEqual(aggregate["num_simulations_completed"], 500)
            self.assertIn("mean_terminal_equity", aggregate)
            self.assertIn("mean_max_drawdown", aggregate)
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)

    def test_optimizer_entrypoint_smoke(self):
        out_dir = self.run_command("optimize", "smoke_optimization.json")
        try:
            results = json.loads((out_dir / "optimization_results.json").read_text())
            self.assertGreater(results["evaluated_candidates"], 0)
            self.assertIn("best_parameters", results)
            self.assertIn("daily_target_atr", results["best_parameters"])
            self.assertIn("long_term_target_atr", results["best_parameters"])
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
