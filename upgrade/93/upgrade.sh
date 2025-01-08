#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common


#copy across new apache conf for mod_status
cat /opt/fpp/etc/apache2.status > /etc/apache2/mods-enabled/status.conf

# Set the reboot flag so the config/cmdline changes will be picked up
    setSetting rebootFlag 1

