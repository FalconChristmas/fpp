#!/bin/bash
#####################################
# Upgrade 165: finish a settings restore that was cut short
#
# After an OS upgrade, a backup restore or a storage copy, the "settings"
# boot action re-applies the saved settings from inside `fppinit start`.
# With an NTP server set, that hung: restarting chrony waited on
# fpp_postnetwork, which waits on fppinit, until fppinit was killed at its
# start timeout. The action had already been taken off BootActions, so the
# settings after the NTP server (HDMI, GPIO fan, services, MQTT broker,
# screensaver, passwords, the CSP) were never applied, and never are.
#
# Nothing on disk records whether a box hit that, so this queues the restore
# once more on every box that could have: one with an NTP server set and
# chrony ordered after fpp_postnetwork. It runs as "settings-repair", which
# leaves the password settings alone where the user never set them, so a
# box whose OS password was changed by hand keeps it. It runs on the next
# boot.
#
# Idempotent - safe to re-run (the action comes off once it has run).
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 165: queue a repair of a settings restore cut short"

if [ -f /etc/fpp/desktop ] || [ -f /etc/fpp/container ]; then
    echo "  Not an FPP OS install - skipping"
    exit 0
fi

if [ ! -f "${CFGFILE}" ] || ! grep -q '^ntpServer\s*=' "${CFGFILE}"; then
    echo "  No NTP server setting - the restore never hung here"
    exit 0
fi

if [ ! -f /etc/systemd/system/chrony.service.d/fpp-defer.conf ]; then
    echo "  chrony is not ordered after fpp_postnetwork - the restore never hung here"
    exit 0
fi

ACTIONS=$(getSetting BootActions)
case ",${ACTIONS}," in
    *,settings,*|*,settings-retry,*|*,settings-repair,*|*,settings-repair-retry,*)
        echo "  A settings restore is already queued"
        exit 0
        ;;
esac

if [ -z "${ACTIONS}" ]; then
    setSetting BootActions "settings-repair"
else
    setSetting BootActions "settings-repair,${ACTIONS}"
fi
echo "  Queued: settings are re-applied on the next boot"
exit 0
