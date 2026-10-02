#!/bin/bash
#####################################
# Upgrade 149: give a Raspberry Pi /dev/spidev1.0 without claiming any pin
#
# New images enable SPI1 through the fpp-spi1-nopins overlay, which creates
# /dev/spidev1.0 with an empty pin state and no chip select, so GPIO19-21 stay
# free until something calls configPin("spi") on P1-35/38/40. The installer
# writes that dtoverlay line into config.txt and installs the .dtbo, but
# neither happens on a device that updates FPP in place, so without this it
# has no spidev1 at all.
#
# Inserted next to FPP's own "dtoverlay=spi0-0cs" so it lands in the [all]
# section the installer wrote. A config.txt without that line has been
# reworked by hand, so it gets the line appended under an explicit [all].
#
# Idempotent: a board already carrying the overlay line changes nothing, and
# the overlay itself is rebuilt and reinstalled either way so the dtoverlay
# line can never name a file that is not there.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR to
# ${FPPDIR}/scripts, so anything derived from it after the source below points
# somewhere else entirely.
UPGDIR=$(cd $(dirname $0) && pwd)
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 149: SPI1 without claiming GPIO19-21"

if [ "${FPPPLATFORM}" != "Raspberry Pi" ]; then
    echo "  Not a Raspberry Pi (${FPPPLATFORM}) - skipping"
    exit 0
fi

CONFIG="${FPPBOOTDIR}/config.txt"
OVERLAYDIR="${FPPBOOTDIR}/overlays"
SRC="${UPGDIR}/../../capes/drivers/pi/fpp-spi1-nopins.dts"
DEST="${OVERLAYDIR}/fpp-spi1-nopins.dtbo"

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
# touched: a dtoverlay line naming a missing or empty .dtbo gains nothing.
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

# Any mention, even commented out, means somebody has already decided.
if grep -qE '^[[:space:]#]*dtoverlay=fpp-spi1-nopins' "${CONFIG}"; then
    echo "  ${CONFIG} already mentions fpp-spi1-nopins - nothing to do"
    exit 0
fi

if grep -qE '^dtoverlay=spi0-0cs$' "${CONFIG}"; then
    sed -i '/^dtoverlay=spi0-0cs$/a dtoverlay=fpp-spi1-nopins' "${CONFIG}"
else
    printf '\n[all]\ndtoverlay=fpp-spi1-nopins\n' >> "${CONFIG}"
fi
sync

if grep -qE '^dtoverlay=fpp-spi1-nopins$' "${CONFIG}"; then
    echo "  ${CONFIG}: added dtoverlay=fpp-spi1-nopins"
    setSetting rebootFlag 1
else
    echo "  ERROR: ${CONFIG} edit did not take"
    exit 1
fi

exit 0
