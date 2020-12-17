#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ -f /etc/default/dnsmasq ]; then
    sed -i -e "s/^IGNORE_RESOLVCONF.*/IGNORE_RESOLVCONF=yes/g" /etc/default/dnsmasq
    sed -i -e "s/#IGNORE_RESOLVCONF.*/IGNORE_RESOLVCONF=yes/g" /etc/default/dnsmasq
    echo "DNSMASQ_EXCEPT=lo" >> /etc/default/dnsmasq

    setSetting rebootFlag 1
fi
