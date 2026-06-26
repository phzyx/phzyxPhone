#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Install the system packages needed to build phzyxPhone and pjproject:
# a C++ toolchain, CMake, pkg-config, Qt (Widgets) and the codec / TLS / audio
# development libraries.
#
# Supports Debian/Ubuntu (apt), Fedora/RHEL (dnf), and Arch (pacman).
# Re-run safe. Uses sudo for the package manager.
#
# Usage:
#   scripts/install-deps.sh            # detect distro and install
#   scripts/install-deps.sh --dry-run  # print the command without running it

set -euo pipefail

DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

run() {
    echo "+ $*"
    [[ "$DRY_RUN" == "1" ]] && return 0
    "$@"
}

# sudo only if we are not already root
SUDO=""
if [[ "$(id -u)" -ne 0 ]]; then
    SUDO="sudo"
fi

detect() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        echo "${ID_LIKE:-$ID}"
    else
        echo "unknown"
    fi
}

FAMILY="$(detect)"
echo ">> Detected distro family: $FAMILY"

case "$FAMILY" in
    *debian*|*ubuntu*)
        # Don't abort if an unrelated third-party repo has a broken Release
        # file; the install below can still proceed from existing indexes.
        run $SUDO apt-get update || echo ">> apt-get update reported errors (continuing)"
        run $SUDO apt-get install -y \
            build-essential cmake pkg-config git \
            qtbase5-dev qtbase5-dev-tools libqt5sql5-sqlite \
            libasound2-dev libssl-dev \
            libopus-dev libgsm1-dev libspeex-dev libspeexdsp-dev
        ;;
    *fedora*|*rhel*|*centos*)
        run $SUDO dnf install -y \
            gcc-c++ cmake pkgconf-pkg-config git make \
            qt5-qtbase-devel \
            alsa-lib-devel openssl-devel \
            opus-devel speex-devel
        echo ">> Note: gsm-devel may require RPM Fusion; GSM is optional."
        ;;
    *arch*)
        run $SUDO pacman -Sy --needed --noconfirm \
            base-devel cmake pkgconf git \
            qt5-base \
            alsa-lib openssl \
            opus speex gsm
        ;;
    *)
        echo "Unsupported distro. Install these manually:" >&2
        echo "  C++ toolchain, cmake, pkg-config, git, make" >&2
        echo "  Qt5 base/Widgets/Sql dev + SQLite driver, ALSA dev, OpenSSL dev" >&2
        echo "  Opus dev, Speex dev, GSM dev (codecs, optional)" >&2
        exit 1
        ;;
esac

echo ">> System dependencies installed."
