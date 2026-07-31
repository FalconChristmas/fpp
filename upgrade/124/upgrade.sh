#!/bin/bash
#####################################
# Upgrade 124: Build the fixed PipeWire GStreamer plugin on existing installs
#
# scripts/build_pipewire_gst_plugin.sh replaces the distro
# gstreamer1.0-pipewire plugin (1.4.2 on Trixie), which crashes in
# on_remove_buffer when a consumer disconnects from a mode=provide
# pipewiresink -- the pattern FPP's persistent Video/Source nodes rely on.
#
# It has existed for a while but has never actually run on any device.  The
# only caller, install_pipewire.sh, gated it on
# `gst-inspect-1.0 pipewiresink | grep -c "provide"`, which matches "element
# provides a clock" and the `(2): provide` mode enum that stock 1.4.2 already
# has.  The count was therefore always non-zero, the gate always reported the
# fix as present, and the build was always skipped.  The gate now lives inside
# the build script as --if-needed and compares versions properly.
#
# The build needs internet access and a few minutes of CPU; it is deliberately
# not fatal, since an offline box must still finish upgrading.
#
# No service restart is done here.  libgstpipewire.so is a GStreamer *client*
# plugin loaded by fppd, not by the pipewire/wireplumber daemons, so restarting
# those would achieve nothing; what matters is fppd, and by the time this runs
# the upgrade is about to rebuild the fppd binary and finish with a reboot.
# (The build script clears the GStreamer registry cache itself.)
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 124: Check/build the fixed PipeWire GStreamer plugin"

if [[ ! -f /etc/debian_version ]]; then
    echo "  Not a Debian system - skipping"
    exit 0
fi

if [ ! -x /opt/fpp/scripts/build_pipewire_gst_plugin.sh ]; then
    echo "  build_pipewire_gst_plugin.sh not found - skipping"
    exit 0
fi

/opt/fpp/scripts/build_pipewire_gst_plugin.sh --if-needed || {
    echo "  WARNING: PipeWire GStreamer plugin build failed (no network?)."
    echo "  Video Input Sources (mode=provide) may crash until this is run:"
    echo "    sudo /opt/fpp/scripts/build_pipewire_gst_plugin.sh"
}

exit 0
