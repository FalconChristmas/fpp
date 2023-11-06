#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ ! -f /etc/fpp/desktop ]; then
    # recopy the latest connect scripts
    cp -af /opt/fpp/etc/networkd-dispatcher/routable.d/* /etc/networkd-dispatcher/routable.d
fi
