# Smoke tests

These are high-level smoke tests for the current vertical slice of the project.

## Guarantees
- every subprocess-driven test uses a hard timeout of 60 seconds
- the suite covers:
  - modular project layout
  - JSON-driven config
  - synthetic GBM + jump-diffusion generation
  - ATR-based strategy thresholds
  - simulation outputs
  - Monte Carlo execution
  - optimizer entrypoint metadata
  - Nix scaffold presence
  - real `dlib` optimizer activation when run through Nix

## Run

```bash
python3 -m unittest tests.test_smoke -v
```

## Nix note

If `nix` is installed, the suite also performs:
- `nix flake show --no-write-lock-file`
- a Nix-shell build/run that verifies the optimizer backend switches to `dlib_find_max_global`
