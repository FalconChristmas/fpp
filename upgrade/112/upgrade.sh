#!/bin/bash
#####################################
# Upgrade 112: Install gstreamer1.0-plugins-base-apps
#
# The file manager shows media duration (and the "Video Info" modal shows
# stream metadata) by probing files in PHP.  This previously relied on the
# ffprobe CLI from ffmpeg.  On GStreamer-based installs that may not be
# present, so the probe now falls back to gst-discoverer-1.0.  That binary
# is provided by the gstreamer1.0-plugins-base-apps package (separate from
# gstreamer1.0-plugins-base), which was not previously a dependency, so
# existing installs lack it and durations show as "Unknown".  Fresh installs
# get this via SD/FPP_Install.sh; this upgrade adds it to already-installed
# systems.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 112: Install gstreamer1.0-plugins-base-apps (gst-discoverer-1.0 for media duration/metadata)"

if [ "${FPPPLATFORM}" != "MacOS" ]; then
    if ! dpkg -l | grep -q "^ii  gstreamer1.0-plugins-base-apps "; then
        echo "  Installing gstreamer1.0-plugins-base-apps"
        apt-get update
        apt-get install -y gstreamer1.0-plugins-base-apps
    else
        echo "  gstreamer1.0-plugins-base-apps already installed"
    fi
else
    echo "  Skipping on MacOS (ffmpeg/ffprobe provides probing there)"
fi

exit 0
