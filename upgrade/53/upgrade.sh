#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common
sed -i 's/ExecStart.*/ExecStart=\/usr\/sbin\/connmand -n --nodnsproxy/g' /lib/systemd/system/connman.service

if [ "${FPPPLATFORM}" == "Raspberry Pi" ]; then

echo "dtoverlay=dwc2" >> /boot/config.txt

cat <<-EOF > /etc/dnsmasq.d/usb.conf
interface=usb0
interface=usb1
port=53
dhcp-authoritative
domain-needed
bogus-priv
expand-hosts
cache-size=2048
dhcp-range=usb0,192.168.7.1,192.168.7.1,2m
dhcp-range=usb1,192.168.6.1,192.168.6.1,2m
listen-address=127.0.0.1
listen-address=192.168.7.2
listen-address=192.168.6.2
dhcp-option=usb0,3
dhcp-option=usb0,6
dhcp-option=usb1,3
dhcp-option=usb1,6
dhcp-leasefile=/var/run/dnsmasq.leases
EOF

fi

systemctl enable dnsmasq

setSetting rebootFlag 1
echo "A reboot will be required to get the tethering packages working"

