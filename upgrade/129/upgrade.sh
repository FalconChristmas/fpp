#!/bin/bash
#####################################
# Upgrade 129: Persist DHCP server leases and set sane lease times
#
# src/boot/FPPINIT_Network.cpp now writes PersistLeases=yes (plus
# DefaultLeaseTimeSec=8h/MaxLeaseTimeSec=8h on the LAN DHCP server) into the
# generated /etc/systemd/network/10-*.network [DHCPServer] sections, so
# leases survive a systemd-networkd restart/reboot instead of being handed
# out fresh every time. Existing installs already have the pre-change
# config deployed, so patch the deployed files in place rather than waiting
# for the next full network config regen (same shape as upgrade/123/126/128
# forcing a recopy of already-generated files).
#
# PersistLeases= (systemd >= 256) replaces the older, differently-named
# option this was first written against; on systemd < 256 there is no DHCP
# lease persistence mechanism at all, so the key is just silently ignored
# there -- harmless no-op, nothing else we can do on that OS version.
# Verified live against systemd 257: leases are stored one JSON file per
# interface under /var/lib/systemd/network/dhcp-server-lease/<iface>, NOT a
# per-lease directory -- do not confuse with the similarly-named but unused
# .../network/leases/ path.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 129: Enable DHCP lease persistence and set lease times"

# Same "FPP doesn't own networking here" gate FPPINIT.cpp's boot "start"
# action uses to skip setupNetwork()/consumePendingDhcpLeaseReset(): desktop
# builds and installs where the admin has explicitly opted out via
# SkipNetworkReset. Patching *.network files and restarting
# systemd-networkd on one of these would reach into config this upgrade
# script has no business touching.
if [[ -f /etc/fpp/desktop ]] || [[ "$(getSetting SkipNetworkReset)" == "1" ]]; then
    echo "FPP - Skipping (SkipNetworkReset/desktop build - FPP is not managing networking here)"
    exit 0
fi

CHANGED=0

for f in /etc/systemd/network/10-*.network; do
    [[ -f "$f" ]] || continue
    grep -q '^\[DHCPServer\]' "$f" || continue

    if ! grep -q '^PersistLeases=' "$f"; then
        sed -i '/^\[DHCPServer\]/a PersistLeases=yes' "$f"
        CHANGED=1
    fi
    if ! grep -q '^DefaultLeaseTimeSec=' "$f"; then
        sed -i '/^\[DHCPServer\]/a DefaultLeaseTimeSec=8h' "$f"
        CHANGED=1
    fi
    if ! grep -q '^MaxLeaseTimeSec=' "$f"; then
        sed -i '/^\[DHCPServer\]/a MaxLeaseTimeSec=8h' "$f"
        CHANGED=1
    fi
done

if [[ "${CHANGED}" == "1" ]]; then
    mkdir -p /var/lib/systemd/network/dhcp-server-lease
    systemctl reload-or-restart systemd-networkd.service
fi

exit 0
