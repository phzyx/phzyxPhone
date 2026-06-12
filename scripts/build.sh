#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Configure and build the phzyxPhone application. Assumes pjproject has
# already been built into third_party/pjproject-install (run
# scripts/build-pjproject.sh first) and that system deps are installed.
#
# Usage:
#   scripts/build.sh [--jobs N] [--debug]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

JOBS="$(nproc 2>/dev/null || echo 2)"
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --jobs)  JOBS="$2"; shift 2 ;;
        --debug) BUILD_TYPE="Debug"; shift ;;
        -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

PREFIX="$REPO_ROOT/third_party/pjproject-install"
if [[ ! -e "$PREFIX/lib/pkgconfig/libpjproject.pc" ]]; then
    echo "!! pjproject not found at $PREFIX" >&2
    echo "!! Run scripts/build-pjproject.sh first." >&2
    exit 1
fi

# Make the local pjproject visible to pkg-config (CMake also does this, but we
# export it here so manual `cmake` invocations work too).
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

cd "$REPO_ROOT"
cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build "-j$JOBS"

echo
echo ">> Build complete: $REPO_ROOT/build/phzyxphone"
echo ">> Run it with:    ./build/phzyxphone"
