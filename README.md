# value-averaging-backtester

C++ backtesting/simulation project for a value-averaging style trading algorithm extracted from the provided Google Doc.

## Current status

The repo now has:
- a modular C++ core
- a thin CLI executable
- Nix as the required development/runtime environment
- high-level smoke tests with 60-second max timeout per test
- a real `dlib`-backed optimization path
- a fixed optimizer objective: `mean_total_return_pct / mean_max_drawdown_pct`
- optimizer controls for:
  - `num_simulations` (500 per evaluation)
  - `optimization_budget` (100 requested evaluations)
  - `optimization_time_budget_seconds` (for quick capped runs such as 30s)

## Key docs

- `docs/requirements.md` — cleaned project requirements
- `docs/source_google_doc_raw.txt` — raw extracted text from the Google Doc
- `docs/architecture.md` — current module layout
- `docs/nix.md` — Nix usage
- `docs/bayesian_optimization.md` — optimizer-library direction

## Important rule

This repo is expected to be developed and tested **inside `nix develop`**.
The smoke tests intentionally fail outside a Nix shell.

## Build and test

Blessed test entrypoint:

```bash
./scripts/test-in-nix.sh
```

Manual path if you want the shell yourself:

```bash
nix develop
cmake -S . -B build
cmake --build build -j2
python3 -m unittest tests.test_smoke -v
```

## CI

GitHub Actions runs the same Nix-based smoke path and `nix build` on pushes/PRs.

## Quick optimization run

A ready-made 30-second config is included at:
- `config/optimization_30s.json`

Run it with:

```bash
nix develop -c ./build-test-nix/cpp_backtester optimize config/optimization_30s.json tmp_opt_30s
cat tmp_opt_30s/optimization_results.json
```

## Nix package build

```bash
nix build .#default
```
