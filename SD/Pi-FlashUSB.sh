#!/bin/bash
#
# Compatibility wrapper.  The flash itself now lives in flash_storage.sh, which is
# shared with the BeagleBone eMMC path.
#
# This used to drive SD/rpi-clone, which decided what to copy by looking at the live
# mount table.  When /boot/firmware was not mounted -- most often because its
# boot-time fsck failed, which is silent because the system boots fine without it --
# the boot partition dropped out of the copy and the flash still reported success.
# flash_storage.sh names the boot filesystem explicitly, refuses to start if it is
# not mounted, and verifies the destination before saying it worked.
#
# Usage (unchanged):  Pi-FlashUSB.sh [-clone] <device>
#
BINDIR=$(cd "$(dirname "$0")" && pwd)

ARGS=()

if [ "$1" = "-clone" ]; then
    # "Copy existing FPP to ..." -- same box, bigger disk: bring media and identity.
    ARGS+=(--clone)
    shift
else
    # "Create new FPP on ..." -- a clean install that can run beside this one.
    ARGS+=(--fresh)
fi

exec "${BINDIR}/flash_storage.sh" -y "${ARGS[@]}" "$1"
