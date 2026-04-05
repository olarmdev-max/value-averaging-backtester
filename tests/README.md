# Smoke tests

These smoke tests are intentionally **Nix-only**.

## Rule
If you run them outside `nix develop`, they should fail immediately.
That is intentional: it prevents drifting back into a half-configured host environment.

## Guarantees
- every subprocess-driven test uses a hard timeout of 60 seconds
- the suite assumes it is already inside a Nix shell
- the suite verifies:
  - modular project layout
  - JSON-driven config
  - synthetic GBM + jump-diffusion generation
  - ATR-based strategy thresholds
  - simulation outputs
  - Monte Carlo execution
  - real `dlib` optimizer activation
  - flake metadata

## Run

Preferred:

```bash
./scripts/test-in-nix.sh
```

Manual:

```bash
nix develop
python3 -m unittest tests.test_smoke -v
```
