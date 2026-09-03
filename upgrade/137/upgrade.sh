#!/bin/bash
#####################################
# Upgrade 137: Install libsamplerate0-dev for AES67 drift correction
#
# AES67Manager compiles its media-clock drift correction -- and with it the
# sourceSilenceFloor gap filling -- on __has_include(<samplerate.h>), and
# src/makefiles/common/setup.mk keys the matching pkg-config flags off the same
# header.  Without it the AES67 send path still builds and runs, but
# "sourceSilenceFloor": true is parsed and then silently does nothing, which
# makes a test report look like a fault in the sender rather than a missing
# build dependency.
#
# libsamplerate0-dev was never in FPP_Install.sh's package list; it arrived on
# most systems only as a transitive dependency of libsdl2-dev, so whether any
# given box has it is luck.  FPP_Install.sh now requests it explicitly; this
# upgrade brings already-installed systems over.  Idempotent -- safe to re-run.
#
# upgrade_config runs before compileBinaries in scripts/git_pull, so a box that
# gains the package here rebuilds with it in the same update.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 137: Install libsamplerate0-dev for AES67 drift correction"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS (no GStreamer, so the AES67 path is not built)"
    exit 0
fi

if [[ ! -f /etc/debian_version ]]; then
    echo "  Not a Debian system - skipping"
    exit 0
fi

if dpkg -l libsamplerate0-dev 2>/dev/null | grep -q '^ii'; then
    echo "  libsamplerate0-dev already installed"
    exit 0
fi

# Best-effort: a box with no internet must not fail the upgrade over this.  It
# keeps building exactly as it does today, just without drift correction, and
# picks the package up on a later update.
echo "  Installing libsamplerate0-dev"
apt-get update > /dev/null 2>&1 || true
if apt-get install -y libsamplerate0-dev; then
    # The header is new but AES67Manager.cpp may be unchanged, so make would not
    # otherwise rebuild it and FPP_HAVE_SAMPLERATE would stay off in the binary.
    touch /opt/fpp/src/mediaoutput/AES67Manager.cpp 2>/dev/null || true
    echo "  Installed - AES67Manager will rebuild with drift correction enabled"
else
    echo "  Could not install libsamplerate0-dev (no network?) - continuing"
    echo "  AES67 will build and run without drift correction until it is installed"
fi

exit 0
