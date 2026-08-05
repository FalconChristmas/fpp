#!/bin/bash
#####################################
# Upgrade 126: Recopy Apache site config for the openapi/plugin-route changes
#
# etc/apache2.site gained proxy rules for /aes67 and /opusrtp (previously
# reachable on fppd but not through Apache at all), a block rule for
# /plugin-apis/internal/* (fppd's introspection-only route), and lost the
# static "RewriteRule ^openapi.json - [L,NC]" passthrough so that
# /api/openapi.json reaches PHP and can merge in live plugin routes instead of
# always serving the checked-in static file. Existing installs still have the
# pre-change config deployed at /etc/apache2/sites-enabled/000-default.conf,
# so none of this takes effect until it's recopied -- same fix shape as
# upgrade/99 and upgrade/123.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 126: Recopy Apache config for openapi/plugin-route changes"

if [[ -f /etc/debian_version ]]; then
    cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

    # Gracefully reload apache config
    gracefullyReloadApacheConf
fi

exit 0
