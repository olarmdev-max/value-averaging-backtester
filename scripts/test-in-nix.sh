#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -z "${IN_NIX_SHELL:-}" ]]; then
  exec nix develop "$ROOT_DIR" -c bash "$ROOT_DIR/scripts/test-in-nix.sh" "$@"
fi

cd "$ROOT_DIR"
python3 -m unittest tests.test_smoke -v
