#!/bin/bash
#####################################
# Upgrade 145: let uart0 exist on a Pi 5 without claiming GPIO14 and GPIO15
#
# A Pi 5 needs uart0 enabled explicitly or there is no /dev/ttyAMA0 at all, and
# the "dtparam=uart0=on" the installer used to write enables the device AND
# muxes both GPIO14 and GPIO15 onto it at boot. Kernel 6.18 turned on strict
# pinmux for pinctrl-rp1, so that claim is now final: gpiod requests against
# either pin fail, FPP's runtime "pinctrl set" is reasserted by the pinctrl
# driver, and the pin cannot be reclaimed by unbinding its owner the way an i2c
# or spi pin can -- the PL011 driver publishes no unbind attribute.
#
# A cape with a button or a pixel string on one of those pins therefore loses
# it, and loses it quietly: the request fails and the input is left reading as
# permanently asserted. The Kulp ShowPlayer 1.0 puts its Test button on P1-10,
# which is GPIO15, so on a Pi 5 it reads as held down and the OLED cycles
# through Testing about once a second. See issue #2965.
#
# Every other board FPP supports leaves the UART unmuxed and claims the pin at
# runtime only when a serial output is configured, which is what lets DMX go out
# of GPIO14 while GPIO15 stays an ordinary GPIO. fpp-uart0-nopins enables uart0
# pointing at an empty pin group, so a Pi 5 behaves the same way.
#
# Idempotent: a board already carrying the overlay line changes nothing, and the
# overlay itself is rebuilt and reinstalled either way so the dtoverlay line can
# never name a file that is not there.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR to
# ${FPPDIR}/scripts, so anything derived from it after the source below points
# somewhere else entirely.
UPGDIR=$(cd $(dirname $0) && pwd)
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 145: uart0 without claiming GPIO14/15"

if [ "${FPPPLATFORM}" != "Raspberry Pi" ]; then
    echo "  Not a Raspberry Pi (${FPPPLATFORM}) - skipping"
    exit 0
fi

CONFIG="${FPPBOOTDIR}/config.txt"
OVERLAYDIR="${FPPBOOTDIR}/overlays"
SRC="${UPGDIR}/../../capes/drivers/pi/fpp-uart0-nopins.dts"
DEST="${OVERLAYDIR}/fpp-uart0-nopins.dtbo"

if [ ! -f "${CONFIG}" ] || [ ! -d "${OVERLAYDIR}" ]; then
    echo "  No ${CONFIG} or ${OVERLAYDIR} - skipping"
    exit 0
fi
if [ ! -f "${SRC}" ]; then
    echo "  ${SRC} not present - skipping"
    exit 0
fi
if ! command -v dtc > /dev/null 2>&1; then
    echo "  dtc not installed - skipping"
    exit 0
fi

# Compile aside and check it before anything is installed or config.txt is
# touched: a dtoverlay line naming a missing or empty .dtbo costs the board its
# UART and gains nothing.
TMPDTBO=$(mktemp) || exit 1
trap 'rm -f "${TMPDTBO}"' EXIT

if ! dtc -O dtb -o "${TMPDTBO}" "${SRC}" > /dev/null 2>&1 || [ ! -s "${TMPDTBO}" ]; then
    echo "  ERROR: could not compile ${SRC} - config.txt left as it was"
    exit 1
fi

if ! cat "${TMPDTBO}" > "${DEST}"; then
    echo "  ERROR: could not install ${DEST} - config.txt left as it was"
    exit 1
fi
chmod 644 "${DEST}" 2>/dev/null
sync
echo "  installed ${DEST}"

# Only a line-anchored dtparam is ours to replace; a commented or indented copy
# is somebody's own note and is left alone.
if grep -qE '^dtparam=uart0=on$' "${CONFIG}"; then
    sed -i 's/^dtparam=uart0=on$/dtoverlay=fpp-uart0-nopins/' "${CONFIG}"
    sync
    if grep -qE '^dtoverlay=fpp-uart0-nopins$' "${CONFIG}"; then
        echo "  ${CONFIG}: dtparam=uart0=on -> dtoverlay=fpp-uart0-nopins"
        setSetting rebootFlag 1
    else
        echo "  ERROR: ${CONFIG} rewrite did not take"
        exit 1
    fi
else
    echo "  ${CONFIG} has no uart0 dtparam to replace - nothing to do"
fi

exit 0
