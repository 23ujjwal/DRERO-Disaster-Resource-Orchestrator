#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

# Catch an existing website/API before building or prompting for credentials.
if command -v ss >/dev/null 2>&1; then
  for port in 8080 3000; do
    if [[ -n "$(ss -H -ltn "sport = :$port")" ]]; then
      echo "Port $port is already in use. Stop the existing DRERO launcher with Ctrl+C before running ./run.sh again." >&2
      exit 1
    fi
  done
fi

if [[ ! -d frontend/node_modules ]]; then
  echo "Frontend dependencies are missing. Run 'cd frontend && npm install' once, then retry." >&2
  exit 1
fi

if [[ -z "${DRERO_DB_PASSWORD:-}" ]]; then
  read -rsp "MySQL password for ${DRERO_DB_USER:-drero_app}: " DRERO_DB_PASSWORD
  printf '\n'
  export DRERO_DB_PASSWORD
fi

cmake -S . -B build
cmake --build build --target drero_server

api_pid=""
frontend_pid=""

cleanup() {
  trap - EXIT INT TERM
  if [[ -n "$api_pid" ]] && kill -0 "$api_pid" 2>/dev/null; then
    kill "$api_pid" 2>/dev/null || true
  fi
  if [[ -n "$frontend_pid" ]] && kill -0 "$frontend_pid" 2>/dev/null; then
    kill "$frontend_pid" 2>/dev/null || true
  fi
  if [[ -n "$api_pid" ]]; then wait "$api_pid" 2>/dev/null || true; fi
  if [[ -n "$frontend_pid" ]]; then wait "$frontend_pid" 2>/dev/null || true; fi
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

./build/drero_server 8080 &
api_pid=$!

# Track Vite itself so cleanup does not leave npm's child server running.
(cd frontend && exec node node_modules/vite/bin/vite.js --host 0.0.0.0) &
frontend_pid=$!

echo "Starting DRERO API and website. Open http://localhost:3000"
echo "Press Ctrl+C to stop both services."

set +e
wait -n "$api_pid" "$frontend_pid"
status=$?
set -e
exit "$status"
