#!/bin/bash
#####################################
# Upgrade 164: disable cloud-init on Raspberry Pi
#
# Raspberry Pi OS ships cloud-init to apply Raspberry Pi Imager's OS
# customisation, and its seed files on the boot partition make it run on
# every boot. It does nothing for FPP -- its network config goes to
# netplan/NetworkManager, which FPP replaces with systemd-networkd -- but it
# holds up sysinit.target, by ~70s on a Pi Zero, and everything ordered
# after it waits too. That pushed the WiFi supplicant's start job past its
# 30s timeout (issue #3178). New images no longer install it; this turns it
# off on existing ones. Takes effect on the next boot.
#
# Remove /etc/cloud/cloud-init.disabled to turn it back on.
#
# Idempotent - safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 164: disable cloud-init"

if [ "${FPPPLATFORM}" != "Raspberry Pi" ]; then
    echo "  Not a Raspberry Pi (${FPPPLATFORM}) - skipping"
    exit 0
fi

if [ -f /etc/fpp/desktop ]; then
    echo "  Desktop install, FPP does not manage the system here - skipping"
    exit 0
fi

# dpkg -s also succeeds for a removed package whose config files remain
if ! dpkg-query -W -f='${Status}' cloud-init 2>/dev/null | grep -q 'install ok installed'; then
    echo "  cloud-init not installed - nothing to do"
    exit 0
fi

if [ -e /etc/cloud/cloud-init.disabled ]; then
    echo "  Already disabled"
    exit 0
fi

mkdir -p /etc/cloud
touch /etc/cloud/cloud-init.disabled
echo "  Disabled (applies after the next reboot)"
exit 0
