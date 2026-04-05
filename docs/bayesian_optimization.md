# Bayesian optimization library decision

## Current state

This project now requires a real `dlib`-backed optimization path.
There is no placeholder optimizer backend anymore.

## Why dlib

- available in nixpkgs
- permissive license
- good maintenance story
- works well for bounded expensive black-box tuning
- integrates cleanly into a Nix-first C++ workflow

## What the optimizer currently searches

- `long_term_target_atr`
- `daily_target_atr`
- `aggressiveness`

## Fixed objective

There is now only one optimizer objective:

- `mean_total_return_pct / mean_max_drawdown_pct`

Interpretation:
- total return is measured as the mean Monte Carlo return percentage versus initial capital
- drawdown is measured as the mean Monte Carlo maximum drawdown percentage magnitude
- the optimizer maximizes their ratio

This keeps the tuning target single-purpose instead of letting multiple competing objective functions drift the search.

## Environment assumption

The optimizer is expected to be built and run inside the repo's Nix environment.
If you step outside that environment, builds/tests should fail rather than silently degrade into a fake fallback path.
