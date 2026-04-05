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

## Supported objectives

- `mean_terminal_equity`
- `positive_run_ratio`
- `drawdown_penalized_equity`
- `risk_adjusted_return`

## Environment assumption

The optimizer is expected to be built and run inside the repo's Nix environment.
If you step outside that environment, builds/tests should fail rather than silently degrade into a fake fallback path.
