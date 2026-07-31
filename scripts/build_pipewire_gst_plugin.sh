#!/bin/bash
#####################################
# build_pipewire_gst_plugin.sh
#
# Build and install the PipeWire GStreamer plugin from source.
#
# The stock Debian Trixie gstreamer1.0-pipewire package (1.4.2) ships a
# GStreamer plugin that crashes in on_remove_buffer when consumers
# disconnect from a pipewiresink running in mode=provide.  This bug
# blocks persistent Video/Source nodes — the core of FPP's video
# input-source architecture.
#
# The fix is present in upstream PipeWire >= 1.6.0 (commit range around
# the mode=provide buffer lifecycle guard in gstpipewiresink.c).  This
# script builds ONLY the GStreamer plugin from PipeWire source and drops
# it in place of the stock .so, leaving the PipeWire daemon, libraries,
# and WirePlumber at their distro versions.
#
# The stock plugin is backed up as libgstpipewire.so.bak-<version>.
#
# Usage:
#   sudo /opt/fpp/scripts/build_pipewire_gst_plugin.sh [--if-needed] [TAG]
#
# TAG defaults to PIPEWIRE_PLUGIN_MIN_VERSION below — override with any
# PipeWire git tag, branch, or commit hash, e.g.:
#   sudo /opt/fpp/scripts/build_pipewire_gst_plugin.sh 1.6.0
#   sudo /opt/fpp/scripts/build_pipewire_gst_plugin.sh master
#
# --if-needed compares the *installed* plugin version against
# PIPEWIRE_PLUGIN_MIN_VERSION and exits 0 without doing anything when it is
# already new enough.  That is what the install/upgrade paths call, so this
# script is the single source of truth for "is the plugin new enough" — the
# callers must not try to answer that themselves.  (They used to: the check was
# `gst-inspect-1.0 pipewiresink | grep -c provide`, which matches "element
# provides a clock" and the mode enum that has existed since long before the
# fix, so it returned non-zero on the stock 1.4.2 plugin and the build was
# skipped on every device it was ever supposed to fix.)
#
# Requires internet access to clone the PipeWire git repository.
#####################################

set -euo pipefail

# Minimum plugin version that carries the mode=provide buffer-lifecycle fix.
PIPEWIRE_PLUGIN_MIN_VERSION="1.6.0"

IF_NEEDED=false
if [ "${1:-}" = "--if-needed" ]; then
    IF_NEEDED=true
    shift
fi

PIPEWIRE_TAG="${1:-${PIPEWIRE_PLUGIN_MIN_VERSION}}"
PIPEWIRE_REPO="https://gitlab.freedesktop.org/pipewire/pipewire.git"
BUILD_DIR="/tmp/pipewire-gst-build"
GST_PLUGIN_DIR=$(pkg-config --variable=pluginsdir gstreamer-1.0 2>/dev/null || echo "/usr/lib/arm-linux-gnueabihf/gstreamer-1.0")
STOCK_VERSION=$(dpkg-query -W -f='${Version}' gstreamer1.0-pipewire 2>/dev/null | cut -d- -f1 || echo "unknown")

# Version of the plugin GStreamer would actually load right now.  gst-inspect
# reports the plugin's own version, so it reads the locally built .so once one
# is installed; dpkg is the fallback for when gstreamer1.0-tools is missing.
installed_plugin_version() {
    local v=""
    if command -v gst-inspect-1.0 >/dev/null 2>&1; then
        v=$(gst-inspect-1.0 pipewiresink 2>/dev/null | awk '/^ *Version/ {print $NF; exit}')
    fi
    [ -z "${v}" ] && v="${STOCK_VERSION}"
    echo "${v}"
}

# True when $1 is older than $2 (plain semver, no epochs/suffixes).
version_lt() {
    [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -1)" = "$1" ]
}

echo "============================================"
echo "FPP — Build PipeWire GStreamer Plugin"
echo "============================================"
echo "  Target PipeWire tag : ${PIPEWIRE_TAG}"
echo "  GStreamer plugin dir: ${GST_PLUGIN_DIR}"
echo "  Stock plugin version: ${STOCK_VERSION}"
echo ""

# --- Root check ---
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This script must be run as root (sudo)."
    exit 1
fi

# --- Runtime compatibility guard ---
# The plugin is compiled against the *checked-out* tree's libpipewire headers,
# but at runtime the loader resolves libpipewire-0.3.so.0 to whatever the
# distro installed.  Build 1.6.0 against a 1.4.2 runtime and the element loads,
# reports itself as 1.6.0, connects, links in the graph -- and then never
# streams: the node stays "suspended" in pw-top, no position is ever produced,
# and playlists play in total silence with nothing logged anywhere.  Verified
# by A/B on an FPP dev Pi 2026-07-31.  So refuse the mismatch by default.
RUNTIME_VERSION=$(pipewire --version 2>/dev/null | awk '/Linked with/ {print $NF; exit}')
[ -z "${RUNTIME_VERSION}" ] && RUNTIME_VERSION=$(dpkg-query -W -f='${Version}' libpipewire-0.3-0t64 2>/dev/null | cut -d- -f1)
TAG_SERIES=$(echo "${PIPEWIRE_TAG}"    | cut -d. -f1,2)
RUN_SERIES=$(echo "${RUNTIME_VERSION}" | cut -d. -f1,2)
echo "  Runtime libpipewire  : ${RUNTIME_VERSION:-unknown}"
if [ -n "${RUN_SERIES}" ] && [ "${TAG_SERIES}" != "${RUN_SERIES}" ]; then
    echo ""
    echo "  ERROR: refusing to build plugin ${PIPEWIRE_TAG} against libpipewire ${RUNTIME_VERSION}."
    echo "         A plugin only works with the libpipewire it was built against."
    echo "         Mismatched builds load and look correct but produce NO AUDIO:"
    echo "         the pipewiresink node never leaves 'suspended'."
    echo ""
    echo "         To get the upstream fixes you must upgrade libpipewire and the"
    echo "         daemon to ${TAG_SERIES}.x as well, not just the plugin."
    echo "         Override at your own risk with FORCE_MISMATCH=1."
    if [ "${FORCE_MISMATCH:-0}" != "1" ]; then
        exit 1
    fi
    echo "  FORCE_MISMATCH=1 set - continuing anyway."
    echo ""
fi

# --- Skip when already new enough (--if-needed) ---
if ${IF_NEEDED}; then
    CURRENT_VERSION=$(installed_plugin_version)
    echo "  Installed plugin version: ${CURRENT_VERSION} (minimum ${PIPEWIRE_PLUGIN_MIN_VERSION})"
    if [ "${CURRENT_VERSION}" != "unknown" ] && \
       ! version_lt "${CURRENT_VERSION}" "${PIPEWIRE_PLUGIN_MIN_VERSION}"; then
        echo "  Plugin is already >= ${PIPEWIRE_PLUGIN_MIN_VERSION} — nothing to do."
        exit 0
    fi
    echo "  Plugin is older than ${PIPEWIRE_PLUGIN_MIN_VERSION} — building from source."
    echo ""
fi

# --- 1. Install build dependencies ---
echo "Step 1: Installing build dependencies..."
apt-get update -q
apt-get install -y -q \
    meson \
    ninja-build \
    git \
    pkg-config \
    libpipewire-0.3-dev \
    libspa-0.2-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev
echo "  Done."

# --- 2. Clone PipeWire source ---
echo ""
echo "Step 2: Cloning PipeWire source (tag: ${PIPEWIRE_TAG})..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
git clone --depth 1 --branch "${PIPEWIRE_TAG}" "${PIPEWIRE_REPO}" pipewire 2>&1 || {
    echo "  Tag '${PIPEWIRE_TAG}' not found — trying as branch/commit..."
    rm -rf pipewire
    git clone "${PIPEWIRE_REPO}" pipewire
    cd pipewire
    git checkout "${PIPEWIRE_TAG}"
    cd ..
}
echo "  Source ready at ${BUILD_DIR}/pipewire"

# --- 3. Configure meson (minimal — GStreamer plugin only) ---
echo ""
echo "Step 3: Configuring meson build..."
cd "${BUILD_DIR}/pipewire"

# Disable everything except the GStreamer plugin to minimize build time.
# Options vary by PipeWire version; we use -Dauto_features=disabled to
# turn off everything, then selectively enable what we need.
meson setup builddir \
    --auto-features=disabled \
    -Dgstreamer=enabled \
    -Dtests=disabled \
    -Dman=disabled \
    -Ddocs=disabled \
    -Dexamples=disabled \
    -Dinstalled_tests=disabled \
    2>&1
echo "  Configuration complete."

# --- 4. Build the GStreamer plugin ---
echo ""
echo "Step 4: Building GStreamer plugin..."
# Build only the GST plugin target to save time
ninja -C builddir src/gst/libgstpipewire.so 2>&1 || {
    echo "  Targeted build failed — falling back to full build..."
    ninja -C builddir 2>&1
}

# Locate the built plugin
BUILT_PLUGIN=$(find builddir -name "libgstpipewire.so" -type f | head -1)
if [ -z "${BUILT_PLUGIN}" ]; then
    echo "ERROR: Could not find built libgstpipewire.so"
    exit 1
fi

BUILT_VERSION=$(strings "${BUILT_PLUGIN}" | grep -oP '^\d+\.\d+\.\d+$' | head -1 || echo "${PIPEWIRE_TAG}")
echo "  Built plugin: ${BUILT_PLUGIN}"
echo "  Plugin version: ${BUILT_VERSION}"

# --- 5. Backup stock plugin and install ---
echo ""
echo "Step 5: Installing plugin..."
DEST="${GST_PLUGIN_DIR}/libgstpipewire.so"

if [ -f "${DEST}" ] && [ ! -f "${DEST}.bak-${STOCK_VERSION}" ]; then
    cp "${DEST}" "${DEST}.bak-${STOCK_VERSION}"
    echo "  Backed up stock plugin → ${DEST}.bak-${STOCK_VERSION}"
fi

cp "${BUILT_PLUGIN}" "${DEST}"
chmod 644 "${DEST}"
echo "  Installed ${DEST}"

# --- 6. Verify ---
echo ""
echo "Step 6: Verifying..."
# Clear GStreamer registry cache so it picks up the new plugin
rm -f /root/.cache/gstreamer-1.0/registry.*.bin 2>/dev/null
rm -f /home/fpp/.cache/gstreamer-1.0/registry.*.bin 2>/dev/null

INSTALLED_VERSION=$(PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp \
    gst-inspect-1.0 pipewiresink 2>/dev/null | grep "Version" | awk '{print $NF}' || echo "unknown")
echo "  gst-inspect-1.0 pipewiresink reports: Version ${INSTALLED_VERSION}"

# Check for mode=provide support
# Assert on the version, not on the presence of the word "provide": the
# mode=provide enum exists in the stock 1.4.2 plugin too (as does "element
# provides a clock"), so a substring check reports success even when nothing
# was replaced.  That false positive is what kept this script from ever running.
if [ "${INSTALLED_VERSION}" = "unknown" ]; then
    echo "  WARNING: could not determine the installed plugin version."
elif version_lt "${INSTALLED_VERSION}" "${PIPEWIRE_PLUGIN_MIN_VERSION}"; then
    echo "  ERROR: plugin still reports ${INSTALLED_VERSION}, expected >= ${PIPEWIRE_PLUGIN_MIN_VERSION}."
    echo "         The new plugin was not picked up -- check ${DEST} and the registry cache."
    exit 1
else
    echo "  Plugin version ${INSTALLED_VERSION} >= ${PIPEWIRE_PLUGIN_MIN_VERSION}: OK"
fi

# --- 7. Clean up ---
echo ""
echo "Step 7: Cleaning up build directory..."
rm -rf "${BUILD_DIR}"
echo "  Done."

echo ""
echo "============================================"
echo "PipeWire GStreamer plugin build complete."
echo "  Installed version: ${INSTALLED_VERSION}"
echo "  Stock backup:      ${DEST}.bak-${STOCK_VERSION}"

# This is a GStreamer *client* plugin: it is dlopen'd by fppd, not by the
# pipewire/wireplumber daemons, so restarting those achieves nothing.  A
# running fppd still has the old .so mapped and keeps using it until it is
# restarted.  Not done automatically -- that would stop a running show.  The
# install and upgrade paths don't need it (nothing is running yet, and an
# upgrade ends in a reboot); a standalone run on a live box does.
if systemctl is-active --quiet fppd 2>/dev/null; then
    echo ""
    echo "  NOTE: fppd is running and still has the previous plugin loaded."
    echo "        Restart it to use the new one:  sudo systemctl restart fppd"
    echo "        (Restarting fpp-pipewire/fpp-wireplumber is NOT needed -- they"
    echo "         do not load this plugin.)"
fi

echo ""
echo "To restore the stock plugin:"
echo "  sudo cp ${DEST}.bak-${STOCK_VERSION} ${DEST}"
echo "============================================"
