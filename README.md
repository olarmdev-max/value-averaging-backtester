# value-averaging-backtester

C++ backtesting/simulation project for a value-averaging style trading algorithm extracted from the provided Google Doc.

## Current status

The repo now has:
- a modular C++ core
- a thin CLI executable
- high-level smoke tests with 60-second max timeout per test
- a Nix flake for reproducible development/build setup
- a real `dlib`-backed optimization path when built through Nix

## Key docs

- `docs/requirements.md` — cleaned project requirements
- `docs/source_google_doc_raw.txt` — raw extracted text from the Google Doc
- `docs/architecture.md` — current module layout
- `docs/nix.md` — Nix usage
- `docs/bayesian_optimization.md` — optimizer-library direction

## Current scope

- JSON-configurable strategy settings
- Synthetic price generation via GBM + jump diffusion
- ATR-based parameterization instead of fixed percentages
- Monte Carlo simulation support
- Modular optimizer backend interface with `dlib` acceleration in Nix environments

## Build locally

```bash
cmake -S . -B build
cmake --build build -j2
```

## Run smoke tests

```bash
python3 -m unittest tests.test_smoke -v
```

## Nix

```bash
nix develop
nix build
```

The flake is the preferred way to work on this repo because it provisions the optimizer dependency stack consistently.
