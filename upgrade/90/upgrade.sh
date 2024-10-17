#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ ! -f /etc/fpp/desktop ]; then
    # recopy the latest connect scripts
    mkdir -p /etc/networkd-dispatcher/initialized.d
    cp -af /opt/fpp/etc/networkd-dispatcher/* /etc/networkd-dispatcher
fi
