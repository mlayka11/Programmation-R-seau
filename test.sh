#!/usr/bin/env bash
set -euo pipefail

# 1) build
if ! [ -x bin/server ]; then
  echo "🔧 Build serveur…"
  make server
fi

# 2) lancer le runner
echo "🧪 Run tests…"
python3 tests/run_scenarios.py
