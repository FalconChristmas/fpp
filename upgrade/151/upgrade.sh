#!/bin/bash
#####################################
# Upgrade 151: Mask the per-user PipeWire stack system-wide
#
# FPP runs its own system PipeWire instance (fpp-pipewire, fpp-wireplumber,
# fpp-pipewire-pulse). The distro's per-user units start a second stack on
# every login - pipewire, wireplumber, pipewire-pulse and filter-chain - which
# reads the same /etc/pipewire config, builds a second sink on the same sound
# card, and D-Bus-activates rtkit-daemon and polkit, which then stay running
# until reboot.
#
# Those units used to be masked only in ~fpp/.config/systemd/user. An fppos
# upgrade syncs /etc but never /home, and skips these upgrade scripts, so a
# device that reached its current OS that way has no masks at all. Masking in
# /etc/systemd/user covers every user and survives an fppos upgrade.
#
# Takes effect at the next login; a user stack that is already running stops
# when its last session ends. The old per-user masks are left in place.
#
# New images get this from FPP_Install.sh. Idempotent - safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 151: Mask the per-user PipeWire services system-wide"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -f /etc/fpp/container ]; then
    echo "  Container - skipping"
    exit 0
fi

if ! systemctl --global mask pipewire.socket pipewire.service \
        pipewire-pulse.socket pipewire-pulse.service \
        wireplumber.service filter-chain.service; then
    # Most likely an admin's own unit file sits at one of those paths. A user
    # stack is a nuisance, not a fault, so don't hold later upgrades back.
    echo "  WARNING: could not mask every per-user PipeWire unit"
fi

exit 0
