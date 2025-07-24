#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#copy across new apache conf to version which includes seperate csp policy file
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

rm -f /opt/fpp/www/.htaccess
rm -f /opt/fpp/www/proxy/.htaccess
rm -f /home/fpp/media/config/.htaccess

cat > /home/fpp/media/config/ui-password-config.conf <<EOF
Allow from All
Satisfy Any
SetEnvIf Host ^ LOCAL_PROTECT=0
EOF

chown fpp:fpp /home/fpp/media/config/ui-password-config.conf

# Gracefully reload apache config
gracefullyReloadApacheConf
