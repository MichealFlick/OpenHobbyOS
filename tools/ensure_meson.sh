#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
VENV_DIR=${1:-"$ROOT/build/meson-venv"}
PYTHON=${PYTHON:-python3}
REQUIREMENTS="$ROOT/tools/meson-requirements.txt"
STAMP="$VENV_DIR/.openhobbyos-meson.stamp"

if command -v meson &>/dev/null; then
    command -v meson
    exit 0
fi

mkdir -p "$(dirname "$VENV_DIR")"

if [[ ! -x "$VENV_DIR/bin/python3" ]]; then
    "$PYTHON" -m venv "$VENV_DIR" 2>/dev/null || true
fi

if [[ -x "$VENV_DIR/bin/pip" ]]; then
    if [[ ! -f "$STAMP" || "$REQUIREMENTS" -nt "$STAMP" ]]; then
        "$VENV_DIR/bin/pip" install --disable-pip-version-check --requirement "$REQUIREMENTS" >&2 2>/dev/null || true
        touch "$STAMP"
    fi
    if [[ -x "$VENV_DIR/bin/meson" ]]; then
        printf '%s\n' "$VENV_DIR/bin/meson"
        exit 0
    fi
fi

echo "error: no meson found (install meson or python3-venv)" >&2
exit 1
