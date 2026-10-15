#!/bin/bash
#####################################
# Upgrade 153: Restore the mDNS host name avahi renames away at boot
#
# avahi can mistake its own link-local announcement for another host when
# IPv6 moves to the global address during boot, and renames itself to
# "<hostname>-2", so <hostname>.local stops resolving. Installs a
# networkd-dispatcher routable hook that puts the name back.
#
# New images get this from FPP_Install.sh. Idempotent - safe to re-run.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
ETCDIR="${UPGDIR}/../../etc"
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 153: Restore the mDNS host name after an avahi self-conflict"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -f /etc/fpp/container ]; then
    echo "  Container - skipping"
    exit 0
fi

mkdir -p /etc/networkd-dispatcher/routable.d
cp -f "${ETCDIR}/networkd-dispatcher/routable.d/avahi_hostname" /etc/networkd-dispatcher/routable.d/
chown root:root /etc/networkd-dispatcher/routable.d/avahi_hostname
chmod 755 /etc/networkd-dispatcher/routable.d/avahi_hostname

exit 0
