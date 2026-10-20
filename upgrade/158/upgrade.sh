#!/bin/bash
#####################################
# Upgrade 158: rebuild pinctrl for the corrected AM335x pin table
#
# pinctrl is built once, when the image is made, so fixes to its pin table
# only reach a git-updated board through here.  UART1_TXD and UART1_RXD had no
# "i2c" mode, though mode 3 on those balls is i2c1.  They are P2-09/P2-11 on
# an original PocketBeagle, where cape detection muxes them to look for an
# EEPROM on i2c1, and "pinctrl -s P2_09 i2c" failed with "Unknown mode".
#
# Idempotent - safe to re-run.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 158: rebuild pinctrl"

if [ "${FPPPLATFORM}" != "BeagleBone Black" ]; then
    echo "  Not a BeagleBone Black (${FPPPLATFORM}) - skipping"
    exit 0
fi

cd "${UPGDIR}/../../capes/drivers/pinctrl" || exit 1
make clean > /dev/null
if ! make; then
    echo "  ERROR: pinctrl did not build - the installed one is left as it was"
    make clean > /dev/null
    exit 1
fi
make install
make clean > /dev/null

exit 0
