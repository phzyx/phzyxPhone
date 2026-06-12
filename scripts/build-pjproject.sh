#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build pjproject (PJSIP) with the PJSUA2 C++ bindings and the codecs that
# phzyxPhone needs, then install it into a self-contained prefix inside this
# repository (third_party/pjproject-install). No root required.
#
# Usage:
#   scripts/build-pjproject.sh [--ref <git-ref>] [--jobs N] [--clean]
#
# Environment overrides:
#   PJPROJECT_REF   git tag/branch to build (default: 2.17)
#   PJPROJECT_REPO  source repo URL (default: https://github.com/pjsip/pjproject.git)

set -euo pipefail

# --- locate repo root (this script lives in <repo>/scripts) ---------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PJPROJECT_REF="${PJPROJECT_REF:-2.17}"
PJPROJECT_REPO="${PJPROJECT_REPO:-https://github.com/pjsip/pjproject.git}"
JOBS="$(nproc 2>/dev/null || echo 2)"
CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ref)   PJPROJECT_REF="$2"; shift 2 ;;
        --jobs)  JOBS="$2"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        -h|--help)
            grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

SRC_DIR="$REPO_ROOT/third_party/pjproject"
PREFIX="$REPO_ROOT/third_party/pjproject-install"

echo ">> Building pjproject '$PJPROJECT_REF'"
echo ">> Source : $SRC_DIR"
echo ">> Prefix : $PREFIX"

if [[ "$CLEAN" == "1" ]]; then
    echo ">> Cleaning previous source and install"
    rm -rf "$SRC_DIR" "$PREFIX"
fi

mkdir -p "$REPO_ROOT/third_party"

# --- fetch source ---------------------------------------------------------
if [[ ! -d "$SRC_DIR/.git" ]]; then
    echo ">> Cloning pjproject ($PJPROJECT_REF)"
    git clone --depth 1 --branch "$PJPROJECT_REF" "$PJPROJECT_REPO" "$SRC_DIR"
else
    echo ">> Source already present; reusing $SRC_DIR"
    echo ">> (use --clean to rebuild from scratch)"
fi

# --- config_site.h: enable the codecs we want for testing -----------------
cat > "$SRC_DIR/pjlib/include/pj/config_site.h" <<'EOF'
/* phzyxPhone build config: maximize codecs for testing.
 * Opus/GSM/iLBC/Speex require their matching system -dev libraries; install
 * them with scripts/install-deps.sh before building. */
#define PJMEDIA_HAS_OPUS_CODEC      1
#define PJMEDIA_HAS_G722_CODEC      1
#define PJMEDIA_HAS_ILBC_CODEC      1
#define PJMEDIA_HAS_GSM_CODEC       1
#define PJMEDIA_HAS_SPEEX_CODEC     1
#define PJMEDIA_HAS_L16_CODEC       1
EOF

# --- configure / build / install -----------------------------------------
pushd "$SRC_DIR" >/dev/null

echo ">> configure"
./configure --prefix="$PREFIX" --enable-shared CFLAGS="-fPIC"

echo ">> make dep"
make dep

echo ">> make (-j$JOBS)"
make "-j$JOBS"

echo ">> make install"
make install

popd >/dev/null

echo
echo ">> pjproject installed to: $PREFIX"
echo ">> pkg-config file:        $PREFIX/lib/pkgconfig/libpjproject.pc"
echo ">> The app's CMake will pick this up automatically."
