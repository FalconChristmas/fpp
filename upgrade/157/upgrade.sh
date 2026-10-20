#!/bin/bash
#####################################
# Upgrade 157: rebuild the base overlay on an original PocketBeagle so the
# PRUs no longer claim P2-28 and eight other P1/P2 pads at boot
#
# The PocketBeagle base dtb muxes those pads to PRU r30/r31 when the PRU
# device probes. P2-28 then sits low, and a cape that gates its i2c2 buffer
# with it (PocketScroller v2) loses its EEPROM and OLED, so detection falls
# back to the built-in config. capes/drivers/bbb/fpp-base-overlay.dts now
# empties the PRUs' pinctrl, but the overlay is only built when an image is,
# so a device that updates FPP in place keeps the old one.
#
# Only an overlay of the trimmed (am335xpm) shape is replaced. An older image
# still has the bone-pinmux-helper overlay its kernel needs, and that overlay
# never had this problem. Nothing else on the BeagleBone Black is affected by
# the change, so those boards are left as they are.
#
# Applies on the next boot. Idempotent - safe to re-run.
#####################################

UPGDIR=$(cd $(dirname $0) && pwd)
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 157: release the PocketBeagle's PRU pins from the base dtb"

if [ "${FPPPLATFORM}" != "BeagleBone Black" ] || ! grep -q 'PocketBeagle' /proc/device-tree/model 2> /dev/null; then
    echo "  Not an original PocketBeagle - skipping"
    exit 0
fi

DEST=/lib/firmware/fpp-base-overlay.dtb
SRCDIR="${UPGDIR}/../../capes/drivers/bbb"

if [ ! -f "${DEST}" ] || ! grep -q '^uboot_overlay_addr0=/lib/firmware/fpp-base-overlay.dtb' /boot/uEnv.txt 2> /dev/null; then
    echo "  ${DEST} is not the boot overlay - skipping"
    exit 0
fi
if grep -q 'bone-pinmux-helper' "${DEST}"; then
    echo "  ${DEST} is the pinmux-helper overlay this kernel needs - skipping"
    exit 0
fi
# A failed upgrade holds back every later one, so an image without the
# device-tree sources the Makefile builds against is a skip, not a failure.
DTD_BASE=$(sed -n 's/^DTD_BASE[[:space:]]*:=[[:space:]]*//p' "${SRCDIR}/Makefile")
if [ -z "${DTD_BASE}" ] || [ ! -d "${DTD_BASE}" ]; then
    echo "  Device-tree sources ${DTD_BASE} not present - ${DEST} left as it was"
    exit 0
fi

# Build in a scratch copy: the Makefile writes next to its sources, and its
# install target would also replace the cape overlay.
TMPDIR=$(mktemp -d) || exit 1
trap 'rm -rf "${TMPDIR}"' EXIT
cp "${SRCDIR}/Makefile" "${SRCDIR}/fpp-base-overlay.dts" "${SRCDIR}/am335x-bone-pins.h" "${TMPDIR}/" || exit 1

if ! make -s -C "${TMPDIR}" fpp-base-overlay.dtb > /dev/null 2>&1 || [ ! -s "${TMPDIR}/fpp-base-overlay.dtb" ]; then
    echo "  ERROR: could not build fpp-base-overlay.dtb - ${DEST} left as it was"
    exit 1
fi

if cmp -s "${TMPDIR}/fpp-base-overlay.dtb" "${DEST}"; then
    echo "  ${DEST} is already current"
    exit 0
fi

if ! cat "${TMPDIR}/fpp-base-overlay.dtb" > "${DEST}"; then
    echo "  ERROR: could not install ${DEST}"
    exit 1
fi
sync
echo "  installed ${DEST} - applies on the next reboot"
setSetting rebootFlag 1

exit 0
