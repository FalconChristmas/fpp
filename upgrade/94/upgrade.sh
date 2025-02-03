#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#copy across new apache conf to version which includes seperate csp policy file
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Set the reboot flag so the config/cmdline changes will be picked up
    setSetting rebootFlag 1