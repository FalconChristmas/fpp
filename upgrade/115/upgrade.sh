#!/bin/bash
#####################################
# Upgrade 115: Repair kiosk-mode breakage caused by the lean Trixie base image.
#
# Three related fixes, all idempotent and safe on a healthy system:
#
# 1. /home/fpp ownership.  adduser only chowns/​populates the home dir when it
#    *creates* it; on boxes where /home/fpp pre-existed (evicted stock UID-1000
#    user, base-image leftovers) or where root-run steps dropped dotfiles, parts
#    of the tree end up root- or other-owned.  fpp then can't write its own home
#    -- e.g. xauth can't create ~/.Xauthority, so kiosk X dies with
#    "timeout in locking authority file".  We recurse to fix it, staying on the
#    home filesystem (-xdev) so an externally-mounted media drive (which may
#    legitimately use a different uid/gid) is left alone.
#
# 2. Stale xauth locks.  If the Pi loses power while xauth holds ~/.Xauthority-c
#    / ~/.Xauthority-l, those lock files persist on disk across the reboot and
#    every later startx then times out trying to lock -> black screen forever.
#    Remove any stale locks.
#
# 3. Missing X packages.  The kiosk install historically used
#    --no-install-recommends, which on the lean Trixie image strips the pieces
#    that make X usable (the setuid console wrapper xserver-xorg-legacy and the
#    input drivers).  FPP_Kiosk.sh now installs the full stack; for already-built
#    kiosk boxes we trigger a re-run of the corrected installer by removing the
#    /etc/fpp/kiosk marker (installKiosk() in FPPINIT re-runs FPP_Kiosk.sh when
#    Kiosk=1 and the marker is absent).  This keeps the package list in one place
#    instead of duplicating it here, and reuses the existing network-resilient
#    retry path.  Only done when something is actually missing, so healthy kiosk
#    boxes are not needlessly reinstalled/rebooted.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 115: Repair kiosk ownership / xauth / X packages"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

NEED_REBOOT=0

# --- 1. Correct /home/fpp ownership (stay on the home filesystem) -------------
if [ -d "${FPPHOME}" ]; then
    echo "  Ensuring ${FPPHOME} is owned by ${FPPUSER}:${FPPGROUP} (excluding mounted media)"
    chown ${FPPUSER}:${FPPGROUP} "${FPPHOME}"
    find "${FPPHOME}" -xdev ! -user ${FPPUSER} -exec chown ${FPPUSER}:${FPPGROUP} {} + 2>/dev/null || true
fi

# --- 2. Remove stale xauth lock files ----------------------------------------
for lock in "${FPPHOME}/.Xauthority-c" "${FPPHOME}/.Xauthority-l"; do
    if [ -e "${lock}" ]; then
        echo "  Removing stale xauth lock ${lock}"
        rm -f "${lock}"
    fi
done

# --- 3. Backfill the full X stack on kiosk boxes that are missing pieces ------
KIOSK_ENABLED=$(getSetting Kiosk)
if [ "${KIOSK_ENABLED}" = "1" ]; then
    MISSING=0
    if [ ! -e /usr/lib/xorg/Xorg.wrap ]; then
        echo "  Kiosk enabled but xserver-xorg-legacy (Xorg.wrap) is missing"
        MISSING=1
    fi
    if ! ls /usr/lib/xorg/modules/input/*.so > /dev/null 2>&1; then
        echo "  Kiosk enabled but no X input driver is installed"
        MISSING=1
    fi

    if [ "${MISSING}" -eq 1 ]; then
        echo "  Triggering a re-run of the corrected FPP_Kiosk.sh installer"
        # Drop the marker so installKiosk() re-runs FPP_Kiosk.sh on next boot,
        # and make sure the service that does so is enabled (so checkInstallKiosk
        # doesn't have to enable+reboot separately).
        rm -f /etc/fpp/kiosk
        if systemctl list-unit-files fpp-install-kiosk.service > /dev/null 2>&1; then
            systemctl enable fpp-install-kiosk > /dev/null 2>&1 || true
        fi
        NEED_REBOOT=1
    else
        echo "  Kiosk X stack looks complete; nothing to reinstall"
    fi
fi

if [ "${NEED_REBOOT}" -eq 1 ]; then
    echo "  Flagging reboot so the corrected kiosk install runs"
    setSetting rebootFlag 1
fi

exit 0
