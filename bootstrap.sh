#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# One-command setup for phzyxPhone:
#   1. install system dependencies (needs sudo)
#   2. build + install pjproject into ./third_party/pjproject-install
#   3. build the application into ./build
#
# Usage:
#   ./bootstrap.sh                 # full setup
#   ./bootstrap.sh --skip-deps     # skip the system-package step
#   ./bootstrap.sh --skip-pjproject# skip rebuilding pjproject

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SKIP_DEPS=0
SKIP_PJ=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-deps)      SKIP_DEPS=1; shift ;;
        --skip-pjproject) SKIP_PJ=1; shift ;;
        -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ "$SKIP_DEPS" == "0" ]]; then
    echo "==> [1/3] Installing system dependencies"
    "$SCRIPT_DIR/scripts/install-deps.sh"
else
    echo "==> [1/3] Skipping system dependencies"
fi

if [[ "$SKIP_PJ" == "0" ]]; then
    echo "==> [2/3] Building pjproject"
    "$SCRIPT_DIR/scripts/build-pjproject.sh"
else
    echo "==> [2/3] Skipping pjproject build"
fi

echo "==> [3/3] Building phzyxPhone"
"$SCRIPT_DIR/scripts/build.sh"

echo
echo "==> Done. Launch with: ./build/phzyxphone"
echo "==> Optional: register the dock/launcher icon with"
echo "    scripts/install-desktop.sh"
