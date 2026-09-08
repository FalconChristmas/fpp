#!/bin/bash
#####################################
# Upgrade 140: Install libgstrtspserver-1.0-dev for the RTSP video output
#
# RTSPOutputManager.cpp compiles on __has_include(<gst/rtsp-server/rtsp-server.h>),
# and src/makefiles/common/setup.mk keys the matching pkg-config flags off the
# same header.  Without it FPP builds and runs exactly as before, just with no
# RTSP output: the config page reports "RTSP not available in fppd", and a
# Network (RTSP) member on a Video Output Group is silently never served.
#
# The package was added to FPP_Install.sh, which covers fresh installs; this
# brings already-installed systems over.  Idempotent -- safe to re-run.
#
# Only libgstrtspserver-1.0-dev is needed.  It pulls in the runtime
# libgstrtspserver-1.0-0, and the separate gstreamer1.0-rtsp package is NOT
# required: that one only provides the rtspclientsink element, which pushes to
# somebody else's server and is not what this does.
#
# upgrade_config runs before compileBinaries in scripts/git_pull, so a box that
# gains the package here rebuilds with it in the same update.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 140: Install libgstrtspserver-1.0-dev for the RTSP video output"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS (the RTSP output is not built there)"
    exit 0
fi

if [[ ! -f /etc/debian_version ]]; then
    echo "  Not a Debian system - skipping"
    exit 0
fi

if dpkg -l libgstrtspserver-1.0-dev 2>/dev/null | grep -q '^ii'; then
    echo "  libgstrtspserver-1.0-dev already installed"
    exit 0
fi

# Best-effort: a box with no internet must not fail the upgrade over this.  It
# keeps building exactly as it does today, just without the RTSP output, and
# the message below says how to add it by hand later.
echo "  Installing libgstrtspserver-1.0-dev"
apt-get update > /dev/null 2>&1 || true
if apt-get install -y libgstrtspserver-1.0-dev; then
    # The header is new but these sources may be unchanged, so make would not
    # otherwise rebuild them and HAS_RTSP_OUTPUT_GSTREAMER would stay off in
    # the binary -- including the fppd.cpp/httpAPI.cpp/command.cpp blocks that
    # start the server, register /rtspoutput and handle reloadRTSPOutputs.
    for f in mediaoutput/RTSPOutputManager.cpp fppd.cpp httpAPI.cpp command.cpp; do
        touch ${FPPDIR}/src/${f} 2>/dev/null || true
    done
    echo "  Installed - fppd will rebuild with the RTSP video output enabled"
else
    echo "  Could not install libgstrtspserver-1.0-dev (no network?) - continuing"
    echo "  FPP will build and run without the RTSP video output."
    echo "  To add it later, run:"
    echo "    sudo apt-get update && sudo apt-get install -y libgstrtspserver-1.0-dev"
    echo "    cd ${FPPDIR}/src && sudo touch mediaoutput/RTSPOutputManager.cpp fppd.cpp httpAPI.cpp command.cpp && sudo make"
    echo "    sudo systemctl restart fppd"
fi

exit 0
