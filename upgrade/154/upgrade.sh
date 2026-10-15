#!/bin/bash
#####################################
# Upgrade 154: rebuild pinctrl for the corrected PocketBeagle 2 pin table
#
# pinctrl is built once, when the image is made, so fixes to its pin table
# only reach a git-updated board through here.  This round corrects the UART
# modes on the PocketBeagle 2 header: P1-36's "uart" mode named a ball that
# does not exist (so muxing it parked the pin and reported success), and
# P2-06, P2-08, P2-09, P2-11, P2-18 and P2-35 had no usable "uart" mode.
#
# Idempotent - safe to re-run.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 154: rebuild pinctrl"

if [ "${FPPPLATFORM}" != "BeagleBone 64" ]; then
    echo "  Not a BeagleBone 64 (${FPPPLATFORM}) - skipping"
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
