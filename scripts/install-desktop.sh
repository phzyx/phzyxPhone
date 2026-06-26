#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# install-desktop.sh - register phzyxPhone with your desktop environment so the
# dock / launcher shows the proper icon (instead of a generic fallback).
#
# Per-user install into ~/.local/share - no root required - pointing the
# launcher at the binary you just built. Re-run after moving the build tree.
# On GNOME/Wayland the icon is matched via the desktop file's StartupWMClass
# and the application's Wayland app-id (set in main.cpp).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RES="${REPO_ROOT}/resources"

# Locate the built binary (override with: BIN=/path/to/phzyxphone ./install-desktop.sh)
BIN="${BIN:-}"
if [[ -z "${BIN}" ]]; then
    for cand in "${REPO_ROOT}/build/phzyxphone" "${REPO_ROOT}/build/bin/phzyxphone"; do
        [[ -x "${cand}" ]] && BIN="${cand}" && break
    done
fi
if [[ -z "${BIN}" || ! -x "${BIN}" ]]; then
    echo "error: could not find the phzyxphone binary. Build it first, or set BIN=..." >&2
    exit 1
fi

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
APPS_DIR="${DATA_HOME}/applications"
ICONS_DIR="${DATA_HOME}/icons/hicolor"

mkdir -p "${APPS_DIR}"

# Install icons into the user's hicolor theme.
for sz in 16 24 32 48 64 128 256; do
    src="${RES}/icons/hicolor/${sz}x${sz}/apps/phzyxphone.png"
    if [[ -f "${src}" ]]; then
        mkdir -p "${ICONS_DIR}/${sz}x${sz}/apps"
        install -m 0644 "${src}" "${ICONS_DIR}/${sz}x${sz}/apps/phzyxphone.png"
    fi
done
if [[ -f "${RES}/icons/hicolor/scalable/apps/phzyxphone.svg" ]]; then
    mkdir -p "${ICONS_DIR}/scalable/apps"
    install -m 0644 "${RES}/icons/hicolor/scalable/apps/phzyxphone.svg" \
        "${ICONS_DIR}/scalable/apps/phzyxphone.svg"
fi

# Write the desktop entry with Exec pointing at the actual binary.
DESKTOP_FILE="${APPS_DIR}/phzyxphone.desktop"
sed "s|^Exec=.*|Exec=${BIN}|" "${RES}/phzyxphone.desktop" > "${DESKTOP_FILE}"
chmod 0644 "${DESKTOP_FILE}"

# Refresh caches (best-effort; harmless if the tools are missing).
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "${APPS_DIR}" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && \
    gtk-update-icon-cache -f -t "${ICONS_DIR}" >/dev/null 2>&1 || true

echo "Installed:"
echo "  ${DESKTOP_FILE}"
echo "  icons -> ${ICONS_DIR}/<size>/apps/phzyxphone.png"
echo
echo "Launcher exec: ${BIN}"
echo "If the dock still shows the old icon, log out/in (or restart GNOME Shell)"
echo "so the compositor re-reads the desktop database."
