# Nix setup

This project now includes a `flake.nix` with:

- a buildable default package
- a development shell with C++ build tools
- Python for the smoke tests
- `dlib` as the practical default optimization dependency
- Eigen for future numerical/optimization work

## Typical commands

Smoke tests through the blessed entrypoint:

```bash
./scripts/test-in-nix.sh
```

Manual shell path:

```bash
nix develop
cmake -S . -B build
cmake --build build -j
python3 -m unittest tests.test_smoke -v
```

Build through Nix:

```bash
nix build
```
