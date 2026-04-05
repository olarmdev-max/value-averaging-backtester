# Bayesian optimization library decision

## Recommendation

For this project, the practical default is **dlib**.

Why:
- already available in nixpkgs
- permissive license
- good maintenance story
- good fit for bounded expensive black-box tuning in a modular C++ app
- much easier operationally than older BO-specific libraries

## Current implementation state

The project now has a real `dlib`-backed optimization path when built in an environment where `dlib` is available (for example through the Nix flake).

Current behavior:
- local non-Nix builds still fall back to the placeholder grid backend if `dlib` is unavailable
- Nix builds enable `dlib::find_max_global`
- the optimizer supports bounded search over:
  - `long_term_target_atr`
  - `daily_target_atr`
  - `aggressiveness`
- the objective is configurable, with support for:
  - `mean_terminal_equity`
  - `positive_run_ratio`
  - `drawdown_penalized_equity`
  - `risk_adjusted_return`

## Important nuance

`dlib` is the best **practical default**, not the most academically pure Bayesian-optimization toolkit.

If later we need deeper control over Gaussian-process internals, acquisition functions, or BO-specific research features:
- **Limbo** is the stronger specialized BO candidate
- **OpenTURNS** is another real BO option, but it is much heavier

## Why BayesOpt is not the default

BayesOpt is technically relevant, but its AGPL/commercial licensing story makes it a poor default choice for this repo.
