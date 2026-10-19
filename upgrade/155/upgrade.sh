#!/bin/bash
#####################################
# Upgrade 155: refuse proxied requests as "local"
#
# The UI password's "Require local" also let in requests that /proxy/ sent
# back to this player's own address: anyone on the LAN skipped the password
# that way.  Install the Apache site that marks proxied requests
# (X-FPP-Proxied) and refuses them as local, and add the same test to an
# existing password config, which is only rewritten when the password
# setting changes.
#
# Idempotent - safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 155: refuse proxied requests as local"

cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

PWCONF="${MEDIADIR}/config/ui-password-config.conf"
if [ -f "${PWCONF}" ] && grep -q '^[[:space:]]*Require local[[:space:]]*$' "${PWCONF}" && ! grep -q 'X-FPP-Proxied' "${PWCONF}"; then
    sed -i "s/^\([[:space:]]*\)Require local[[:space:]]*\$/\1Require local\n\1Require expr \"%{HTTP:X-FPP-Proxied} == ''\"/" "${PWCONF}"
fi

gracefullyReloadApacheConf
