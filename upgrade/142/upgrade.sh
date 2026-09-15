#!/bin/bash
#####################################
# Upgrade 142: Re-deploy the FPP systemd units from the git tree
#
# etc/systemd/*.service is copied into /lib/systemd/system exactly once, by
# FPP_Install.sh at install time.  Nothing refreshes it afterwards, so every
# change to a unit file has only ever reached freshly flashed images -- a
# version upgrade updates the copy in /opt/fpp and leaves the unit systemd
# actually reads frozen at whatever the image shipped with.
#
# The two paths that look like they cover this do not:
#
#   FPPINIT_Audio.cpp   refreshes fpp-pipewire.service, fpp-wireplumber.service
#                       and fpp-pipewire-pulse.service on boot -- those three
#                       only, and only when the PipeWire backend is in use.
#   install_pipewire.sh re-copies fppd.service too, but nothing invokes it; it
#                       is documented as a manual recovery step.
#
# What made this visible: fppd.service gained LimitRTPRIO=95 so AES67Manager's
# packet-pacing thread can go SCHED_FIFO.  It is a ceiling, not a request, so
# without it the sched_setscheduler call just fails EPERM and the sender pushes
# packets up to a full ptime late every few thousand -- the exact jitter tail
# the limit was added to remove.  On an upgraded box the limit never arrived.
#
# Only units that are ALREADY installed are refreshed.  A unit missing from
# /lib/systemd/system means this platform's install never wanted it (fppoled
# and fpprtc are Pi/BBB-only, the PipeWire units are written by
# install_pipewire.sh), and introducing one here would put a file on the system
# that nothing enabled and nobody asked for.  Same policy FPPINIT_Audio uses.
#
# Copying a unit does not start, stop, enable or disable anything -- systemd
# only re-reads it on daemon-reload, and a changed resource limit applies at the
# service's next start.  So this sets the reboot flag rather than restarting
# fppd in-band, for the same reason upgrade 138 does: the upgrade's own output
# is streaming over a connection fppd/apache are serving.
#
# Idempotent -- re-running with everything already current copies nothing.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 142: Refresh the installed systemd units from /opt/fpp"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS - no systemd"
    exit 0
fi

SRCDIR="/opt/fpp/etc/systemd"
DSTDIR="/lib/systemd/system"

if [ ! -d "${SRCDIR}" ] || [ ! -d "${DSTDIR}" ]; then
    echo "  ${SRCDIR} or ${DSTDIR} missing - skipping"
    exit 0
fi

CHANGED=0

for SRC in ${SRCDIR}/*.service; do
    [ -f "${SRC}" ] || continue
    UNIT=$(basename "${SRC}")
    DST="${DSTDIR}/${UNIT}"

    # Not installed on this platform -- see the header.
    if [ ! -f "${DST}" ]; then
        continue
    fi

    if cmp -s "${SRC}" "${DST}"; then
        continue
    fi

    echo "  Updating ${UNIT}"
    cp -f "${SRC}" "${DST}"
    CHANGED=1
done

if [ ${CHANGED} -eq 0 ]; then
    echo "  All installed units already current"
    exit 0
fi

echo "  Running systemctl daemon-reload"
systemctl daemon-reload

# The units are on disk and systemd has re-read them, but a service already
# running keeps the limits it started with.  Flag the reboot instead of
# restarting here.
setSetting rebootFlag 1

exit 0
