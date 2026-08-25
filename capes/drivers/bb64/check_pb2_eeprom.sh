#!/bin/bash

# Validate -- and where possible repair -- the PocketBeagle2 on-board EEPROM.
#
# Some boards leave the factory with the EEPROM only partly programmed.  The write
# is a two-step process (board template, then serial number) and either step can
# fail, so both of these turn up in the field:
#
#   - magic header (bytes 0-1) not AA55, i.e. nothing was written at all
#   - valid header but the serial number (bytes 52-55) still all FF
#
# Neither is cosmetic.  u-boot's SPL only applies the 1GB DDRSS timings when the
# EEPROM identifies the board as the industrial variant, so a PocketBeagle2
# Industrial with an unreadable EEPROM comes up as a base board running on half its
# RAM.  The fix scripts merge a template in, overwriting only bytes that are still
# 0xFF, which makes them safe to run in either failure mode and leaves a
# factory-programmed serial alone.
#
# A third mode has also been seen: no EEPROM on the i2c bus at all.  This one is not
# an EEPROM fault, because there is no EEPROM part on a PocketBeagle2 -- an on-board
# MSPM0L1105 emulates a 24c32 at 0x50, and the kernel's at24 driver talks to that.
# If the MSPM0's own firmware was never flashed, 0x50 does not ACK, at24's probe-time
# test read fails, and the driver never binds.  Nothing this script can write will
# help, because the thing that would serve the write is what is missing.  So it
# refuses -- see the exit codes.
#
# That is recoverable, but not from here: the MSPM0's ROM bootstrap loader answers on
# i2c at 0x48 once it is reset with the backdoor asserted, and the AM62 drives both
# of those lines itself (the NRST and BSL_Invoke pins, already in FPP's pin table).
# So no external programmer is needed -- but reflashing starts with a mass erase, so
# it must never be attempted on a board whose MSPM0 is merely misbehaving.
#
# The same MSPM0 also emulates the ad7291 ADC at 0x20, so the two appear and
# disappear together.  That is what separates "the MSPM0 is not running at all" from
# "it is running but not serving 0x50", and the two need different people to look at
# them, so the check reports which one it found.
#
# Its own i2c address (0x48, a DT node with no Linux driver) is useless here: it does
# not ACK there even on a healthy board, so its silence proves nothing.  Whether the
# emulated devices bound is the only signal that means anything.
#
# But "did not bind" only means something once the driver exists to bind.  at24 and
# ad7291 are modules, loaded by udev off each client's MODALIAS -- and the boot-time
# flasher runs as init with no udev, where an entirely healthy board presents the
# unprogrammed-MSPM0 signature exactly.  Hence the modprobe below, and the separate
# verdict for "the driver is not there" ahead of any verdict about the board.
#
# Usage:  check_pb2_eeprom.sh [--quiet]
#
# Exit codes:
#   0  EEPROM is good, or was repaired and re-verified.  Safe to continue.
#   1  EEPROM is missing, unwritable, or inconsistent with the hardware.  The
#      caller must abort -- flashing a board in this state produces one that boots
#      with the wrong memory size or does not identify itself at all.

# Both sysfs paths are overridable so the failure modes -- absent EEPROM, refused
# write, wrong board ID -- can be exercised against fixtures.  Nothing in FPP
# sets these; they default to the real hardware.
EEPROM=${PB2_EEPROM:-/sys/bus/i2c/devices/0-0050/eeprom}
EMMC_TYPE=${PB2_EMMC_TYPE:-/sys/block/mmcblk0/device/type}
# The i2c client directory exists whenever the DT node was instantiated, even if
# no driver bound to it.  That is what separates "the MSPM0 is not answering" from
# "the i2c bus never came up".
I2CDEV=${PB2_I2CDEV:-/sys/bus/i2c/devices/0-0050}
# The ADC the same MSPM0 emulates.  A bound driver here means the MSPM0 is alive.
ADCDEV=${PB2_ADCDEV:-/sys/bus/i2c/devices/0-0020}
MODEL_FILE=${PB2_MODEL:-/proc/device-tree/model}
DRIVERDIR=$(dirname "$(readlink -f "$0")")

# The i2c bus is probed asynchronously.  When this runs from the boot-time flasher
# the EEPROM node can still be a second or two away, and "absent" is a verdict that
# aborts the flash -- so it has to be the settled answer, not the first one.
EEPROM_WAIT=10
# Likewise the eMMC, which is the fallback identity check when the header is blank.
EMMC_WAIT=10

info() { [ "${QUIET}" = "y" ] || echo "$*"; }
fatal() { echo "EEPROM: $*" >&2; exit 1; }

QUIET="n"
[ "$1" = "--quiet" ] && QUIET="y"

# Only the PocketBeagle2 family has this EEPROM at this address with this layout.
# Every other BeagleBone 64 board reaching this script wants to be left alone.
MODEL=$(tr -d '\0' < "${MODEL_FILE}" 2>/dev/null || true)
case "${MODEL}" in
    *PocketBeagle2*) ;;
    *)
        info "EEPROM: '${MODEL:-unknown}' is not a PocketBeagle2; nothing to check."
        exit 0
        ;;
esac

# udev is what loads the driver module for an i2c client, off the MODALIAS in its
# uevent.  The boot-time flasher runs this script from init= with no udev at all,
# and at24/ad7291 are both modules -- so on that path the clients exist with no
# driver bound, which from sysfs is indistinguishable from an MSPM0 that never came
# up.  Load them here so the verdict below is about the board and not about the
# environment the script was called from.  A no-op when they are already loaded,
# built in, or when running against fixtures.
if [ -z "${PB2_EEPROM}${PB2_I2CDEV}${PB2_ADCDEV}" ] && command -v modprobe >/dev/null 2>&1; then
    modprobe at24 2>/dev/null || true
    modprobe ad7291 2>/dev/null || true
fi

# The EEPROM has to be present before anything else can be decided.
i=0
while [ ! -f "${EEPROM}" ] && [ "${i}" -lt "${EEPROM_WAIT}" ]; do
    i=$((i + 1))
    sleep 1
done
if [ ! -f "${EEPROM}" ]; then
    # Both of these end the flash, but they point at different repairs, and the
    # message is the only thing whoever picks the board up gets to work from.
    #
    # Before any of them: if the at24 driver is not even registered, nothing could
    # have bound to 0x50 whatever the hardware did, and every verdict below would be
    # an accusation against a board that was never asked.  That is a broken image or
    # a missing module, not a broken board.
    if [ -z "${PB2_EEPROM}${PB2_I2CDEV}${PB2_ADCDEV}" ] && [ ! -d /sys/bus/i2c/drivers/at24 ]; then
        fatal "the at24 driver is not loaded, so nothing can bind to the EEPROM at 0x50.

  This says nothing about the board -- the driver that would read it is missing.
  It is a module in the FPP kernel and is normally loaded by udev; check that
  /lib/modules/$(uname -r) matches the running kernel.

  Re-run this once at24 is loaded before judging the hardware."
    fi
    if [ -d "${I2CDEV}" ]; then
        # The client exists but at24 did not bind, so 0x50 did not ACK its probe
        # read.  On a PocketBeagle2 that EEPROM is emulated by the on-board
        # MSPM0L1105 -- there is no EEPROM part that could be faulty -- and the same
        # MSPM0 emulates the ADC at 0x20.  Whether THAT bound says which of two very
        # different faults this is.
        if [ -e "${ADCDEV}/driver" ]; then
            fatal "the EEPROM at 0x50 is not answering (no ${EEPROM} after ${EEPROM_WAIT}s).

  The on-board MSPM0L1105 emulates both this EEPROM and the ADC at 0x20, and the
  ADC did bind -- so the MSPM0 is running, but it is not serving 0x50.  That is
  not the usual unprogrammed-board signature; it points at the MSPM0 firmware
  rather than at the absence of it.

  Either way there is nothing at 0x50 to write to, so this cannot be repaired
  from here.  This board must not be flashed or shipped."
        fi
        fatal "neither of the MSPM0's emulated devices came up (no ${EEPROM} after ${EEPROM_WAIT}s,
  and nothing bound at 0x20 either).

  On a PocketBeagle2 the identity EEPROM is emulated by the on-board MSPM0L1105,
  which also emulates the ADC at 0x20 -- there is no EEPROM part to be faulty.
  Both being silent means the MSPM0 itself is not running, which is what an
  MSPM0 that was never flashed at the factory looks like.

  Nothing this script can write will fix it: with 0x50 silent there is nothing
  to write to.  The MSPM0 has to be reflashed first, over its ROM bootstrap
  loader at i2c 0x48 -- the AM62 drives the NRST and BSL_Invoke lines itself, so
  this needs no external programmer, but it does start with a mass erase.

  Until then u-boot cannot identify the board and will configure it as the
  512MB base variant.  This board must not be flashed or shipped."
    fi
    fatal "no i2c device at ${I2CDEV}, and so no EEPROM at ${EEPROM}.

  The client is created from the device tree, so its absence means the i2c bus
  itself did not come up -- a wrong or missing device tree rather than an
  unprogrammed board.  Check the boot log for i2c@20000000.

  This board must not be flashed or shipped."
fi

# An EEPROM that answers the probe read but fails a real one lands here otherwise as
# an empty header, which reads as "blank" -- and sends a board whose i2c is failing
# down the repair path to abort later with a misleading complaint about the write.
GOT=$(dd if="${EEPROM}" bs=1 count=64 2>/dev/null | wc -c)
[ "${GOT}" = "64" ] || fatal "${EEPROM} exists but a 64-byte read returned ${GOT} bytes.

  The at24 driver bound, so 0x50 answered at probe time, but it is not serving
  reads now.  Nothing can be written to a device in this state.

  This board must not be flashed or shipped."

read_field() { dd if="${EEPROM}" bs=1 skip="$1" count="$2" 2>/dev/null; }

MAGIC=$(read_field 0 2 | xxd -p)
# The unit number is the six ASCII digits at offset 50..55, which is the field
# the fix scripts assign as a whole.  Testing only the low four (the old
# behaviour) calls a board healthy when 50..51 are still 0xFF, and get_uuid
# then rejects the serial anyway -- so check the whole field.
UNIT=$(read_field 50 6)
# Offset 46 holds the board ID: "PB2I" on the industrial board, "PB20" on the base
# one.  It survives the "valid header, blank serial" failure mode, so read it
# whenever the header is there at all.
BOARDID=""
[ "${MAGIC}" = "aa55" ] && BOARDID=$(read_field 46 4)

# Which board is this really?
#
# The device-tree model string is NOT usable here: it reads "BeagleBoard.org
# PocketBeagle2" on the industrial board as well as on the base one.  The hardware
# tell is the eMMC -- populated on the industrial, an empty footprint on the base
# board -- and it is the only evidence left when the header is blank.
# Memoised: this is asked twice, and the second caller must not pay the wait again
# -- on a base board, where the answer is a legitimate "no", every second of it is
# spent waiting for hardware that is not fitted.
EMMC_ANSWER=""
emmc_present() {
    local i=0
    if [ -z "${EMMC_ANSWER}" ]; then
        EMMC_ANSWER="no"
        while [ "${i}" -lt "${EMMC_WAIT}" ]; do
            if [ "$(cat "${EMMC_TYPE}" 2>/dev/null)" = "MMC" ]; then
                EMMC_ANSWER="yes"
                break
            fi
            i=$((i + 1))
            sleep 1
        done
    fi
    [ "${EMMC_ANSWER}" = "yes" ]
}

if [ -n "${BOARDID}" ]; then
    case "${BOARDID}" in
        PB2I) INDUSTRIAL=true  ;;
        PB20) INDUSTRIAL=false ;;
        *)    fatal "unrecognised board ID '${BOARDID}' at offset 46 (expected PB2I or PB20)." ;;
    esac
elif emmc_present; then
    INDUSTRIAL=true
    info "EEPROM: header blank; eMMC is present, so this is the industrial board."
else
    INDUSTRIAL=false
    info "EEPROM: header blank and no eMMC found, so this is the base board."
fi

# An EEPROM that claims to be the base board on hardware that has eMMC is the one
# broken state the merge cannot repair: the wrong bytes are already programmed, and
# the fix scripts only fill in bytes that are still 0xFF.  Say so rather than
# running a fix that would report success while changing nothing.
if [ "${INDUSTRIAL}" = "false" ] && [ -n "${BOARDID}" ] && emmc_present; then
    fatal "board ID reads PB20 (base board) but this board has eMMC populated,
  which only the industrial variant does.

  The EEPROM is fully programmed with the wrong identity, so the merge-based fix
  cannot correct it -- it only fills in bytes that are still 0xFF.  u-boot will
  keep configuring this board for 512MB.

  Rewriting it needs a deliberate, destructive write; refusing to guess here."
fi

NEEDS_FIX=false
if [ "${MAGIC}" != "aa55" ]; then
    info "EEPROM: header invalid (expected aa55, got ${MAGIC:-empty})"
    NEEDS_FIX=true
elif ! [[ "${UNIT}" =~ ^[0-9]{6}$ ]]; then
    info "EEPROM: header valid but the unit number is blank or malformed"
    NEEDS_FIX=true
fi

if [ "${NEEDS_FIX}" = "false" ]; then
    info "EEPROM: valid (${BOARDID}, magic=${MAGIC}, unit=${UNIT})"
    exit 0
fi

if [ "${INDUSTRIAL}" = "true" ]; then
    FIX="${DRIVERDIR}/fix_pb2i_eeprom.sh"
else
    FIX="${DRIVERDIR}/fix_pb2_eeprom.sh"
fi
[ -x "${FIX}" ] || [ -f "${FIX}" ] || fatal "repair script ${FIX} is missing."

info "EEPROM: repairing with $(basename "${FIX}")"
/bin/bash "${FIX}" || fatal "$(basename "${FIX}") failed."

# Re-read and re-check.  The write goes through sysfs to a part that can be write
# protected or simply absent behind a stuck bus, and every one of those failures
# looks like a successful `dd`.  Without this the caller is told the board was
# repaired on exactly the boards where it was not.
MAGIC=$(read_field 0 2 | xxd -p)
UNIT=$(read_field 50 6)
BOARDID=$(read_field 46 4)

[ "${MAGIC}" = "aa55" ] || fatal "still no valid header after the repair (magic=${MAGIC:-empty}).
  The EEPROM did not accept the write.  This board must not be flashed."
[[ "${UNIT}" =~ ^[0-9]{6}$ ]] || fatal "unit number is still blank or malformed after the repair (unit=${UNIT}).
  The EEPROM did not accept the write.  This board must not be flashed."

if [ "${INDUSTRIAL}" = "true" ]; then
    [ "${BOARDID}" = "PB2I" ] || fatal "board ID reads '${BOARDID}' after the industrial repair.
  This board would still boot as a 512MB base board."
else
    [ "${BOARDID}" = "PB20" ] || fatal "board ID reads '${BOARDID}' after the base-board repair."
fi

info "EEPROM: repaired and verified (${BOARDID}, magic=${MAGIC}, unit=${UNIT})"
exit 0
