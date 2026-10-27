#!/bin/bash
#####################################
# Upgrade 161: let the File Manager's player send a Content-Length
#
# Listen and View play api/file/<dir>/<name>?play=1 in the page, with HTTP
# range support. Since Apache 2.4.59 (the CVE-2024-24795 fix), mod_proxy_fcgi
# drops the Content-Length a PHP reply sets and sends the body chunked, so
# every 206 went out without a length, which Safari and iOS won't play or seek.
# The new apache conf sets ap_trust_cgilike_cl for those requests only.

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Copy across the new apache conf.
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Gracefully reload apache config
gracefullyReloadApacheConf
