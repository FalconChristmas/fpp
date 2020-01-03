#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

cp /opt/fpp/etc/systemd/fppd.service /lib/systemd/system
cp /opt/fpp/etc/systemd/fpp_postnetwork.service /lib/systemd/system

systemctl daemon-reload
systemctl reenable fppd
systemctl enable fpp_postnetwork.service

setSetting rebootFlag 1
echo "A reboot will be required to get the screen blanking working"
