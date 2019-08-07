#!/bin/bash
#####################################
# This script enables mod-header on Apache 2 which is needed to set the CORS headers
# for stuff in the API directory

# Enable Apache modules
echo "Enabling Apache Header modules"
a2enmod headers
a2enmod proxy_html
a2enmod proxy_http2

# Copy new Apache site config into place
echo "Copying new Apache config into place"
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" < ${FPPDIR}/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common
setSetting rebootFlag 1
echo "A reboot will be required to get the new Apache config working working"

