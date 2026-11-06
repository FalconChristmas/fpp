#!/bin/bash
#####################################
# Upgrade 162: rebuild pinctrl for the PocketBeagle 2 BOOT button
#
# pinctrl is built once, when the image is made, so additions to its pin table
# only reach a git-updated board through here.  The BOOT button drives the
# GPIO0_28 bootmode pad (ball T24) and now has a "BOOT" pin, so
# "pinctrl -q BOOT" can show how it is muxed.  fppd reads the button through
# gpiod and does not need this to use it.
#
# Idempotent - safe to re-run.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 162: rebuild pinctrl"

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
