# value-averaging-backtester

C++ backtesting/simulation project for a value-averaging style trading algorithm extracted from the provided Google Doc.

## Current status

The repo now has:
- a modular C++ core
- a thin CLI executable
- Nix as the required development/runtime environment
- high-level smoke tests with 60-second max timeout per test
- a real `dlib`-backed optimization path

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

```bash
nix develop
cmake -S . -B build
cmake --build build -j2
python3 -m unittest tests.test_smoke -v
```

## Nix package build

```bash
nix build .#default
```
