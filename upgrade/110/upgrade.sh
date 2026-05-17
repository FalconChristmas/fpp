#!/bin/bash
#####################################
# Upgrade 110: Install gstreamer1.0-alsa
#
# FPP's GStreamer audio pipeline prefers the alsasink element for
# direct-ALSA (non-PipeWire) playback — autoaudiosink probes pulsesink
# first and fails slowly in ALSA-only mode.  alsasink is provided by the
# gstreamer1.0-alsa package, which was not previously a dependency, so
# existing installs lack it (the code falls back to autoaudiosink, but
# the alsasink fast path is preferred).  Fresh installs get this via
# SD/FPP_Install.sh; this upgrade adds it to already-installed systems.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 110: Install gstreamer1.0-alsa (alsasink for direct-ALSA playback)"

if [ "${FPPPLATFORM}" != "MacOS" ]; then
    if ! dpkg -l | grep -q "^ii  gstreamer1.0-alsa "; then
        echo "  Installing gstreamer1.0-alsa"
        apt-get update
        apt-get install -y gstreamer1.0-alsa
    else
        echo "  gstreamer1.0-alsa already installed"
    fi
else
    echo "  Skipping on MacOS (no ALSA)"
fi

exit 0
