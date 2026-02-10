#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#copy across new apache conf to version which fixes the ordering for proxy rewrite rules
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Gracefully reload apache config
gracefullyReloadApacheConf
