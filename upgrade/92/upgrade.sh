#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#enable mod expires
a2enmod expires
#copy across new apache conf
cat /opt/fpp/etc/apache2.site >/etc/apache2/sites-enabled/000-default.conf
# Set the reboot flag so the config/cmdline changes will be picked up
    setSetting rebootFlag 1