#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common


sed -i -e "s/PreferredTechnologies\(.*\)/PreferredTechnologies=wifi,ethernet/" /etc/connman/main.conf


setSetting rebootFlag 1
echo "A reboot will be required to get the eth0/wlana0 interfaces configured"
