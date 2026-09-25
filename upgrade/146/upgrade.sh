#!/bin/bash
#####################################
# Upgrade 146: Reinstall mp3gain if an OS upgrade left it broken
#
# In-place OS upgrades from FPP 8/9 (pre-trixie) to FPP 10 (Debian 13
# trixie) can leave a stale mp3gain binary on disk: trixie moved libmpg123
# from 1.31.x to 1.32.x across the 64-bit time_t transition
# (libmpg123-0 -> libmpg123-0t64) while mp3gain stayed at the same upstream
# version (1.6.2-2), so the .fppos rsync's size+mtime comparison can skip
# the binary -- the same class of stale file upgradeOS-part2.sh already
# force-cleans for ping/librtmp/bc. The stale binary then dies against the
# new library:
#
#   mp3gain: symbol lookup error: mp3gain: undefined symbol: mpg123_decode_frame
#
# (issues #2873, #2986). Clean FPP 10 installs are unaffected -- only boxes
# that came forward via an .fppos upgrade carry the stale file -- and
# re-extracting the package against the current library fixes it, so detect
# the broken binary and reinstall it. Idempotent -- safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 146: Reinstall mp3gain if broken by the OS upgrade"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [[ ! -f /etc/debian_version ]]; then
    echo "  Not a Debian system - skipping"
    exit 0
fi

if ! command -v mp3gain > /dev/null 2>&1; then
    echo "  mp3gain not installed - nothing to do"
    exit 0
fi

# A working binary prints its version; a stale one dies in the loader with
# "symbol lookup error ... mpg123_decode_frame" (rc=127). Match the loader
# failure rather than the exit code alone so a future usage-text change
# cannot false-positive us into an apt reinstall.
MP3GAIN_PROBE=$(mp3gain -v 2>&1)
MP3GAIN_RC=$?
if [ ${MP3GAIN_RC} -ne 127 ] && ! echo "${MP3GAIN_PROBE}" | grep -q "symbol lookup error"; then
    echo "  mp3gain works - nothing to do"
    exit 0
fi

echo "  mp3gain is broken (${MP3GAIN_PROBE}) - reinstalling"

# Best-effort: a box with no network keeps the broken binary and retries on
# a later update, so this must never fail the upgrade.
export DEBIAN_FRONTEND=noninteractive
apt-get update > /dev/null 2>&1 || true
REINSTALLED=false
if apt-get install --reinstall -y mp3gain; then
    REINSTALLED=true
fi
# If the stale side is the library rather than the binary, re-extract
# whichever libmpg123 runtime this OS actually has installed.
for LIBPKG in $(dpkg -l 'libmpg123*' 2>/dev/null | awk '/^ii/ {print $2}' || true); do
    if apt-get install --reinstall -y "${LIBPKG}"; then
        REINSTALLED=true
    fi
done

if [ "${REINSTALLED}" = "true" ]; then
    if mp3gain -v > /dev/null 2>&1; then
        echo "  mp3gain reinstalled and working"
    else
        echo "  mp3gain reinstalled but still failing - reinstall manually:"
        echo "    sudo apt-get install --reinstall -y mp3gain"
    fi
else
    echo "  Could not reinstall mp3gain (no network?) - continuing"
    echo "  To fix it later, run:"
    echo "    sudo apt-get update && sudo apt-get install --reinstall -y mp3gain"
fi

exit 0
