import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = REPO_ROOT / "build-test-nix"
BINARY = BUILD_DIR / "cpp_backtester"
TIMEOUT_SECONDS = 60


class SmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.environ.get("IN_NIX_SHELL"):
            raise AssertionError(
                "Smoke tests must be run inside `nix develop`. "
                "Refusing to run outside a Nix shell."
            )

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

    def test_running_inside_nix_shell(self):
        self.assertTrue(os.environ.get("IN_NIX_SHELL"))

    def test_modular_layout_smoke(self):
        expected = [
            REPO_ROOT / "include" / "cpp_backtester" / "config.hpp",
            REPO_ROOT / "include" / "cpp_backtester" / "price_generator.hpp",
            REPO_ROOT / "include" / "cpp_backtester" / "simulator.hpp",
            REPO_ROOT / "include" / "cpp_backtester" / "monte_carlo.hpp",
            REPO_ROOT / "include" / "cpp_backtester" / "optimizer.hpp",
            REPO_ROOT / "src" / "config.cpp",
            REPO_ROOT / "src" / "price_generator.cpp",
            REPO_ROOT / "src" / "simulator.cpp",
            REPO_ROOT / "src" / "monte_carlo.cpp",
            REPO_ROOT / "src" / "optimizer.cpp",
            REPO_ROOT / "docs" / "architecture.md",
            REPO_ROOT / "flake.nix",
            REPO_ROOT / "flake.lock",
        ]
        for path in expected:
            self.assertTrue(path.exists(), f"missing module file: {path}")

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
            self.assertIn("mean_total_return_pct", aggregate)
            self.assertIn("mean_max_drawdown_pct", aggregate)
            self.assertIn("return_over_drawdown_ratio", aggregate)
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)

    def test_optimizer_uses_real_dlib_backend(self):
        out_dir = self.run_command("optimize", "smoke_optimization.json")
        try:
            results = json.loads((out_dir / "optimization_results.json").read_text())
            self.assertTrue(results["dlib_available"])
            self.assertEqual(results["optimizer_backend"], "dlib_find_max_global")
            self.assertEqual(results["objective_name"], "mean_total_return_pct / mean_max_drawdown_pct")
            self.assertEqual(results["requested_evaluations"], 100)
            self.assertEqual(results["monte_carlo_sims_per_evaluation"], 500)
            self.assertGreater(results["evaluated_candidates"], 0)
            self.assertEqual(
                results["total_monte_carlo_simulations_completed"],
                results["evaluated_candidates"] * 500,
            )
            self.assertIn("best_parameters", results)
            self.assertIn("daily_target_atr", results["best_parameters"])
            self.assertIn("long_term_target_atr", results["best_parameters"])
        finally:
            shutil.rmtree(out_dir, ignore_errors=True)

    def test_flake_metadata_smoke(self):
        subprocess.run(
            ["nix", "flake", "show", "--no-write-lock-file", str(REPO_ROOT)],
            check=True,
            timeout=TIMEOUT_SECONDS,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
