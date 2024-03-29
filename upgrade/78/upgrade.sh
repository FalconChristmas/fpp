#!/bin/bash
#####################################

# Copy new Apache site config into place
echo "Copying updated Apache config into place"
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" < ${FPPDIR}/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common
setSetting rebootFlag 1
echo "A reboot will be required for the Apache config to come into effect"
