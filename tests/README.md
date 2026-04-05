# Smoke tests

These are high-level smoke tests for the current vertical slice of the project.

## Guarantees
- each subprocess-driven test uses a hard timeout of 60 seconds
- tests verify the top-level goals are wired end-to-end:
  - JSON-driven config
  - synthetic GBM + jump-diffusion generation
  - ATR-based strategy thresholds
  - simulation outputs
  - Monte Carlo execution
  - optimization entrypoint

## Run

```bash
python3 -m unittest tests.test_smoke -v
```
