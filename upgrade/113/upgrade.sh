#!/bin/bash
#####################################
# Upgrade 113: Restore root ownership of /etc/networkd-dispatcher.
#
# upgrade/80 and upgrade/90 copy the networkd-dispatcher hooks from /opt/fpp
# into /etc with "cp -af", which preserves the source owner instead of
# creating root-owned files.  On an install where /opt/fpp is not owned by
# root, that leaves the hook directories non-root-owned, and networkd-dispatcher
# then refuses to run the hooks:
#
#   ERROR:invalid permissions on /etc/networkd-dispatcher/routable.d.
#   expected mode=0o755, uid=0, gid=0; got mode=0o755, uid=1001, gid=1001
#
# which breaks the routable.d/ntpd time-sync hook.  This restores root:root
# ownership and restarts the service.  It only acts when it finds a non-root
# owner, so it is a no-op on a healthy system.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 113: Restore root ownership of /etc/networkd-dispatcher"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -d /etc/networkd-dispatcher ]; then
    if [ -n "$(find /etc/networkd-dispatcher ! -uid 0 -o ! -gid 0 2>/dev/null | head -1)" ]; then
        echo "  Fixing ownership on /etc/networkd-dispatcher"
        chown -R root:root /etc/networkd-dispatcher
        if systemctl is-enabled networkd-dispatcher >/dev/null 2>&1; then
            echo "  Restarting networkd-dispatcher"
            systemctl restart networkd-dispatcher 2>/dev/null || true
        fi
    else
        echo "  /etc/networkd-dispatcher already root-owned"
    fi
fi

exit 0
