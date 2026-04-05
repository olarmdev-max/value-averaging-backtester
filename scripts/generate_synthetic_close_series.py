#!/usr/bin/env python3
import argparse
import csv
import json
import math
import random
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a close-only synthetic CSV using GBM + jump diffusion")
    parser.add_argument("config", help="Path to config JSON")
    parser.add_argument("output", help="Output CSV path")
    args = parser.parse_args()

    cfg = json.loads(Path(args.config).read_text())
    seed = int(cfg.get("seed", 123))
    num_days = int(cfg.get("num_days", 252))
    start_price = float(cfg.get("start_price", 100.0))
    mu = float(cfg.get("mu", 0.08))
    sigma = float(cfg.get("sigma", 0.30))
    jump_probability = float(cfg.get("jump_probability", 0.02))
    jump_mean = float(cfg.get("jump_mean", -0.03))
    jump_std = float(cfg.get("jump_std", 0.08))

    rng = random.Random(seed)
    dt = 1.0 / 252.0
    close = start_price

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["close"])
        for _ in range(num_days):
            diffusion = (mu - 0.5 * sigma * sigma) * dt + sigma * math.sqrt(dt) * rng.gauss(0.0, 1.0)
            jump_term = 0.0
            if rng.random() < jump_probability:
                jump_term = jump_mean + jump_std * rng.gauss(0.0, 1.0)
            close = max(close * math.exp(diffusion + jump_term), 0.01)
            writer.writerow([f"{close:.6f}"])

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
