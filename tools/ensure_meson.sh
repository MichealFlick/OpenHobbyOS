#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
VENV_DIR=${1:-"$ROOT/build/meson-venv"}
PYTHON=${PYTHON:-python3}
REQUIREMENTS="$ROOT/tools/meson-requirements.txt"
STAMP="$VENV_DIR/.openhobbyos-meson.stamp"

mkdir -p "$(dirname "$VENV_DIR")"

if [[ ! -x "$VENV_DIR/bin/python3" ]]; then
    "$PYTHON" -m venv "$VENV_DIR"
fi

if [[ ! -f "$STAMP" || "$REQUIREMENTS" -nt "$STAMP" ]]; then
    "$VENV_DIR/bin/pip" install --disable-pip-version-check --requirement "$REQUIREMENTS" >&2
    touch "$STAMP"
fi

printf '%s\n' "$VENV_DIR/bin/meson"
