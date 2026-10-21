#!/bin/bash
#####################################
# Upgrade 103: install the distribution's PHP security updates
#
# PHP2Shell (CVE-2026-109618, CVE-2026-109619) lets an unauthenticated request
# run code on any PHP that serves web requests, PHP-FPM included, which is how
# FPP's web UI runs. A git update never touches the OS packages, so without
# this only a new image or fppos would bring the fixed PHP.
#
# scripts/php_security_update upgrades only the installed php8.x packages, to
# what the configured apt sources carry, and leaves php-fpm running (an update
# started from the web UI runs inside php-fpm's cgroup, and the package's
# restart would kill it): the reboot flag is set instead.
#
# Where the player is offline it enables fpp-php-security-update.timer to
# retry after each boot and daily until the fix is installed; the timer turns
# itself off then.
#
# Raspberry Pi OS (32-bit) for Debian 12 has never published a PHP update:
# raspbian.raspberrypi.com still carries the 8.2.7 bookworm shipped with, so
# a 32-bit Pi on FPP 9.5 can't get the fix from apt at all. It gets the
# upload-progress workaround below and a pointer to FPP 10, whose Raspberry
# Pi OS does publish PHP's security updates; no timer, which would retry for
# ever.
#
# Best effort: never holds back the upgrades after it. Idempotent.
#####################################

# Captured under our own name: scripts/common reassigns BINDIR and FPPDIR.
UPGDIR=$(cd $(dirname $0) && pwd)
UNITDIR="${UPGDIR}/../../etc/systemd"
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 103: install the PHP security updates"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -f /etc/fpp/container ]; then
    echo "  Container - skipping"
    exit 0
fi

if ! command -v apt-get > /dev/null 2>&1; then
    echo "  No apt-get - skipping"
    exit 0
fi

# PHP's own workaround for CVE-2026-109619's session path, set where an
# application or .user.ini can't turn it back on. FPP never uses upload
# progress, so it stays off for good, fixed PHP or not. FPP_Install.sh sets it
# on new installs.
MITIGATION='php_admin_value[session.upload_progress.enabled] = 0'
for POOL in /etc/php/*/fpm/pool.d/www.conf; do
    [ -f "${POOL}" ] || continue
    grep -q '^php_admin_value\[session\.upload_progress\.enabled\]' "${POOL}" && continue
    cp -p "${POOL}" "${POOL}.fpp103"
    printf '\n; FPP: upload progress is never used; off as the CVE-2026-109619 workaround\n%s\n' "${MITIGATION}" >> "${POOL}"
    VER=$(echo "${POOL}" | cut -d/ -f4)
    if command -v php-fpm${VER} > /dev/null 2>&1 && ! php-fpm${VER} -t > /dev/null 2>&1; then
        echo "  WARNING: php-fpm${VER} rejected the change to ${POOL} - put it back"
        mv -f "${POOL}.fpp103" "${POOL}"
        continue
    fi
    rm -f "${POOL}.fpp103"
    echo "  Turned off session.upload_progress in ${POOL}"
    setSetting rebootFlag 1
done

"${UPGDIR}/../../scripts/php_security_update"
RC=$?
if [ ${RC} -eq 0 ]; then
    exit 0
fi

if [ "${OSID}" = "raspbian" ]; then
    echo "  WARNING: Raspberry Pi OS (32-bit) for Debian 12 has no PHP security updates."
    echo "  WARNING: Upgrade to FPP 10 (About -> Upgrade OS) to get the PHP security fix."
    exit 0
fi

# Only where the distribution will publish the fix under the same upstream
# version (Debian). On others the check can't tell a backported fix apart,
# and would retry for ever.
if [ "${OSID}" != "debian" ]; then
    echo "  WARNING: install your distribution's PHP security update (apt-get upgrade)"
    exit 0
fi

if cp -f "${UNITDIR}/fpp-php-security-update.service" "${UNITDIR}/fpp-php-security-update.timer" /lib/systemd/system/ \
    && systemctl daemon-reload \
    && systemctl enable --now fpp-php-security-update.timer; then
    echo "  Will retry after each boot and daily until the fixed PHP is installed"
else
    echo "  WARNING: could not enable fpp-php-security-update.timer"
    echo "  WARNING: apply the OS update (fppos) or run apt-get upgrade to install the PHP security fix"
fi

exit 0
