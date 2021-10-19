#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ -f /etc/systemd/journald.conf ]; then
    sed -i -e "s/^.*SystemMaxUse.*/SystemMaxUse=32M/g" /etc/systemd/journald.conf

    setSetting rebootFlag 1
fi
