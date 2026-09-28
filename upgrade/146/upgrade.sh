#!/bin/bash
#####################################
# Upgrade 146: install xinput on devices that already have kiosk mode
#
# The kiosk now rotates the Touch Display 2 touchscreen live with xinput
# instead of through an xorg.conf.d TransformationMatrix. SD/FPP_Kiosk.sh
# installs xinput, but it only runs when kiosk is first enabled or has to be
# reinstalled, so a device that already has kiosk would never get it.
#
# Best effort: start_kiosk.sh keeps the old TransformationMatrix in place
# until xinput is present, so a device that is offline right now loses
# nothing, and a failure here must not hold back the upgrades after it.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 146: install xinput for kiosk touch rotation"

if [ ! -f /etc/fpp/kiosk ]; then
    echo "  Kiosk not installed - skipping"
    exit 0
fi

if command -v xinput > /dev/null 2>&1; then
    echo "  xinput already installed"
    exit 0
fi

APT_OPTS="-o DPkg::Lock::Timeout=60"
apt-get $APT_OPTS update
if apt-get $APT_OPTS install -y xinput; then
    echo "  xinput installed"
else
    echo "  WARNING: could not install xinput - kiosk keeps its existing touch rotation"
fi
apt-get clean

exit 0
