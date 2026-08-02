#!/bin/bash
#####################################
# Upgrade 125: Restore the stock PipeWire GStreamer plugin
#
# upgrade/124 (now removed) built and installed upstream PipeWire 1.6.0's
# GStreamer plugin over the distro's 1.4.2 one.  That combination is broken:
# the plugin is compiled against 1.6.0's in-tree libpipewire but at runtime
# links to the distro libpipewire-0.3.so.0 (1.4.2), and audio playback stops
# working entirely -- the pipewiresink stream node stays "suspended" in pw-top,
# GStreamer never produces a position, and playlists run silently with
# seconds_elapsed stuck at 0.  There is no error in fppd.log; the UI reports
# the media as playing.
#
# Any device that ran upgrade 124 is in that state, and cannot be fixed by
# re-running 124 (its config version has already advanced).  Restore the stock
# plugin from the backup the build script left behind.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 125: Restore the stock PipeWire GStreamer plugin"

if [[ ! -f /etc/debian_version ]]; then
    echo "  Not a Debian system - skipping"
    exit 0
fi

GST_PLUGIN_DIR=$(pkg-config --variable=pluginsdir gstreamer-1.0 2>/dev/null)
if [ -z "${GST_PLUGIN_DIR}" ] || [ ! -f "${GST_PLUGIN_DIR}/libgstpipewire.so" ]; then
    echo "  No GStreamer PipeWire plugin installed - nothing to do"
    exit 0
fi

DEST="${GST_PLUGIN_DIR}/libgstpipewire.so"

# Newest backup the build script left, if any.
BACKUP=$(ls -1t "${DEST}".bak-* 2>/dev/null | head -1)
if [ -z "${BACKUP}" ]; then
    echo "  No stock backup present - plugin was never replaced, nothing to do"
    exit 0
fi

CURRENT=$(gst-inspect-1.0 pipewiresink 2>/dev/null | awk '/^ *Version/ {print $NF; exit}')
STOCK=$(dpkg-query -W -f='${Version}' gstreamer1.0-pipewire 2>/dev/null | cut -d- -f1)
echo "  Installed plugin: ${CURRENT:-unknown}   distro package: ${STOCK:-unknown}"

# Only act when the loaded plugin is genuinely not the distro one.  Comparing
# against the package version rather than a hardcoded 1.6.0 keeps this correct
# if the distro itself moves on.
if [ -n "${CURRENT}" ] && [ -n "${STOCK}" ] && [ "${CURRENT}" = "${STOCK}" ]; then
    echo "  Already running the distro plugin - nothing to do"
    exit 0
fi

echo "  Restoring ${BACKUP} -> ${DEST}"
cp -a "${BACKUP}" "${DEST}"
chmod 644 "${DEST}"

# The registry caches the plugin's metadata; clear both users that matter
# (fppd runs as root, the web UI shells out as fpp).
rm -f /root/.cache/gstreamer-1.0/registry.*.bin 2>/dev/null
rm -f /home/fpp/.cache/gstreamer-1.0/registry.*.bin 2>/dev/null

RESTORED=$(gst-inspect-1.0 pipewiresink 2>/dev/null | awk '/^ *Version/ {print $NF; exit}')
echo "  Plugin now reports: ${RESTORED:-unknown}"
echo "  (Takes effect in fppd on its next restart, which this upgrade ends with.)"

exit 0
