#!/bin/bash
#####################################
# Upgrade 159: install the current chrony reconnect hook
#
# The networkd-dispatcher routable hook that resyncs chrony after a network
# reconnect used to end with a bare "chronyc makestep". That step cleared the
# kernel's NTP sync status, so timedatectl and the Health Check reported "NTP
# daemon not synchronized" for up to ~68 minutes after every reconnect. The
# hook was fixed in the tree, but only an install or upgrade 116 copies it
# into /etc, so a git-updated player kept the old one.
#
# Skipped where chrony isn't installed: the hook only drives chronyc.
#
# New images get this from FPP_Install.sh. Idempotent - safe to re-run.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
ETCDIR="${UPGDIR}/../../etc"
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 159: install the current chrony reconnect hook"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -f /etc/fpp/container ]; then
    echo "  Container - skipping"
    exit 0
fi

if [ ! -x /usr/bin/chronyc ]; then
    echo "  chrony is not installed - skipping"
    exit 0
fi

HOOK=/etc/networkd-dispatcher/routable.d/chrony
mkdir -p /etc/networkd-dispatcher/routable.d
if cmp -s "${ETCDIR}/networkd-dispatcher/routable.d/chrony" "${HOOK}"; then
    echo "  ${HOOK} already current"
elif ! cp -f "${ETCDIR}/networkd-dispatcher/routable.d/chrony" "${HOOK}"; then
    echo "  ERROR: could not install ${HOOK}"
    exit 1
fi
# networkd-dispatcher skips a hook that is not root-owned, or is writable by
# anyone else.
chown root:root "${HOOK}"
chmod 755 "${HOOK}"

exit 0
