#!/usr/bin/env bash
set -euo pipefail
# Default is dry run; pass --confirm explicitly to remove validated outputs.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/tools/phase0.py" clean "$@"
