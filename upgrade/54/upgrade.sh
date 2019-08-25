#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common


sed -i 's/^NetworkInterfaceBlacklist.*/NetworkInterfaceBlacklist=SoftAp0,usb0,usb1/g' /etc/connman/main.conf
if ! grep -q NetworkInterfaceBlacklist "/etc/connman/main.conf"; then
    echo "NetworkInterfaceBlacklist=SoftAp0,usb0,usb1" >> /etc/connman/main.conf
fi

cp /opt/fpp/etc/systemd/connman/fppd.service /lib/systemd/system
systemctl daemon-reload
systemctl reenable fppd

setSetting rebootFlag 1
echo "A reboot will be required to get the tethering packages working"
