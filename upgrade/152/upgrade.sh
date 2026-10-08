#!/bin/bash
#####################################
# Upgrade 152: Don't let a missing WiFi adapter stall boot for 90s
#
# A WiFi client configured for an adapter that is not plugged in held
# network.target, fpp_postnetwork and fppd for the device unit's 90s default
# timeout. Installs a wpa_supplicant@ drop-in that gives up after 30s, and a
# udev rule that starts the supplicant whenever the adapter does appear.
#
# New images get this from FPP_Install.sh. Idempotent - safe to re-run.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
ETCDIR="${UPGDIR}/../../etc"
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 152: Stop a missing WiFi adapter from stalling boot"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -f /etc/fpp/container ]; then
    echo "  Container - skipping"
    exit 0
fi

mkdir -p "/etc/systemd/system/wpa_supplicant@.service.d"
cp -f "${ETCDIR}/systemd/wpa_supplicant@.service.d/fpp-missing-adapter.conf" "/etc/systemd/system/wpa_supplicant@.service.d/"
cp -f "${ETCDIR}/udev/rules.d/80-fpp-wpa-supplicant.rules" /etc/udev/rules.d/
systemctl daemon-reload
udevadm control --reload

exit 0
