# value-averaging-backtester

C++ backtesting/simulation project for a value-averaging style trading algorithm extracted from the provided Google Doc.

## Current status

The repo now has:
- a modular C++ core
- a thin CLI executable
- Nix as the required development/runtime environment
- high-level smoke tests with 60-second max timeout per test
- a real `dlib`-backed optimization path
- a fixed optimizer objective: `mean_cagr_pct / mean_max_drawdown_pct`
- a native rolling-window optimizer for dated historical CSVs with parallel window execution
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
- `docs/price_file_schema.md` — close-only CSV input schema

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

## File-backed input mode

You can now pass one or more close-only CSV files directly on the command line:

```bash
nix develop -c ./build-test-nix/cpp_backtester simulate config/smoke_simulation.json out data/SPY.csv
nix develop -c ./build-test-nix/cpp_backtester optimize config/optimization_30s.json out data/SPY.csv data/QQQ.csv
```

Rolling optimization expects dated CSVs:

```bash
nix develop -c ./build-test-nix/cpp_backtester optimize-rolling config/rolling_optimization_30s.json out data/SPY_dated.csv data/QQQ_dated.csv
```

Schema details are documented in `docs/price_file_schema.md`.

## Helper scripts

- `scripts/generate_synthetic_close_series.py` — create a close-only CSV from GBM + jump diffusion
- `scripts/download_yahoo.py` — download a close-only or dated CSV from Yahoo Finance (for example `SPY`)

Example:

```bash
nix develop
python3 scripts/download_yahoo.py SPY data/SPY.csv --start 2020-01-01
./build-test-nix/cpp_backtester optimize config/optimization_30s.json out data/SPY.csv
```

For rolling optimization, include dates:

```bash
python3 scripts/download_yahoo.py SPY data/SPY_dated.csv --start 2020-01-01 --include-date
./build-test-nix/cpp_backtester optimize-rolling config/rolling_optimization_30s.json out data/SPY_dated.csv data/QQQ_dated.csv
```

## CI

GitHub Actions runs the same Nix-based smoke path and `nix build` on pushes/PRs.

## Quick optimization run

A ready-made 30-second config is included at:
- `config/optimization_30s.json`
- `config/rolling_optimization_30s.json`

Run it with:

```bash
nix develop -c ./build-test-nix/cpp_backtester optimize config/optimization_30s.json tmp_opt_30s
cat tmp_opt_30s/optimization_results.json
```

## Nix package build

```bash
nix build .#default
```
