#!/bin/bash
#####################################
# Upgrade 148: Stop running the exim daemon
#
# FPP and its plugins only hand mail to the sendmail binary (mail -s, PHP
# mail()), which delivers it from the submitting process, so the exim daemon
# has nothing to do. It costs real CPU anyway: with no TLS certificate
# configured, exim 4.98's daemon generates a self-signed RSA certificate at
# startup and, because that certificate only lives an hour, again at the first
# queue-run wakeup after it expires (every 90 minutes in practice) -- 5-10s of
# CPU each time on a single-core board. Its SMTP listener on 127.0.0.1:25 is
# unused attack surface as well.
#
# fpp-exim-queue.timer takes over the one useful thing the daemon did,
# retrying deferred mail, and costs nothing while the queue is empty.
#
# New images get this from FPP_Install.sh. Idempotent -- safe to re-run.
#####################################

# Locate the unit files from our own directory, under a name scripts/common
# doesn't touch: it derives FPPDIR from $0 (upgrade/, not the tree root) and
# reassigns BINDIR and SRCDIR from that.
UPGDIR=$(cd $(dirname $0) && pwd)
UNITDIR="${UPGDIR}/../../etc/systemd"
BINDIR="${UPGDIR}"
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 148: Replace the exim daemon with a queue-retry timer"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

if [ -f /etc/fpp/container ]; then
    echo "  Container - skipping"
    exit 0
fi

if [ ! -x /usr/sbin/exim4 ]; then
    echo "  exim4 not installed - skipping"
    exit 0
fi

if ! cp -f ${UNITDIR}/fpp-exim-queue.service ${UNITDIR}/fpp-exim-queue.timer /lib/systemd/system/; then
    # Without the retry timer, deferred mail would never be retried, so keep
    # the daemon rather than half-apply this. Not fatal: the box stays exactly
    # as it was, and later upgrades shouldn't be held back by it.
    echo "  WARNING: could not install fpp-exim-queue units - leaving exim4 running"
    exit 0
fi
systemctl daemon-reload
systemctl enable --now fpp-exim-queue.timer
systemctl disable --now exim4.service

exit 0
