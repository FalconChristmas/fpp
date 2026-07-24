#!/bin/bash
#####################################
# Upgrade 123: Recopy Apache site config for the new /variables proxy rule
#
# etc/apache2.site gained a RewriteRule bridging /api/variables -> drogon's
# /variables REST handler (same pattern as the existing /gpio rule), needed by
# the Variables page and the If/Set Variable commands. Existing installs still
# have the pre-Variables config deployed at
# /etc/apache2/sites-enabled/000-default.conf, so /api/variables falls through
# to the generic PHP catch-all route and 404s instead of reaching fppd. Same
# fix shape as upgrade/99.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 123: Recopy Apache config for /variables proxy rule"

if [[ -f /etc/debian_version ]]; then
    cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

    # Gracefully reload apache config
    gracefullyReloadApacheConf
fi

exit 0
