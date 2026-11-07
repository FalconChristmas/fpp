#!/bin/bash
#####################################
# Upgrade 163: at24.write_timeout=1000 on the Raspberry Pi kernel command line
#
# Cape EEPROMs emulated in a microcontroller's flash hold the bus (NAK) while
# a flash sector is erased: tens of ms, and a few hundred on worn flash.
# at24's default 25ms write timeout gives up first, so writing the cape
# EEPROM fails.  The BeagleBone 64 extlinux.conf already sets this; new Pi
# images get it from FPP_Install.sh.  Takes effect on the next reboot.
#
# Idempotent - safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 163: at24.write_timeout on the Pi kernel command line"

if [ "${FPPPLATFORM}" != "Raspberry Pi" ]; then
    echo "  Not a Raspberry Pi (${FPPPLATFORM}) - skipping"
    exit 0
fi

CMDLINE="/boot/firmware/cmdline.txt"
if [ ! -f "${CMDLINE}" ]; then
    echo "  ${CMDLINE} not found - skipping"
    exit 0
fi

if grep -q 'at24\.write_timeout=' "${CMDLINE}"; then
    echo "  Already set"
    exit 0
fi

sed -i 's/$/ at24.write_timeout=1000/' "${CMDLINE}"
echo "  Added at24.write_timeout=1000 (applies after the next reboot)"
exit 0
