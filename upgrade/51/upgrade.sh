#!/bin/bash
#####################################

# Install dnsmasq hostapd for the Wifi tethering support
apt-get -y install dnsmasq hostapd

sed -i 's/ExecStart.*/ExecStart=\/usr\/sbin\/connmand -n --nodnsproxy/g' /lib/systemd/system/connman.service

# replace entry already there
sed -i 's/^DAEMON_CONF.*/DAEMON_CONF="\/etc\/hostapd\/hostapd.conf"/g' /etc/default/hostapd
if ! grep -q etc/hostapd/hostapd.conf "/etc/default/hostapd"; then
    # not there, append after the commented out default line
    sed -i 's/^#DAEMON_CONF\(.*\)/#DAEMON_CONF\1\nDAEMON_CONF="\/etc\/hostapd\/hostapd.conf"/g' /etc/default/hostapd
fi
if ! grep -q etc/hostapd/hostapd.conf "/etc/default/hostapd"; then
    # default line not there, just append to end of file
    echo "DAEMON_CONF=\"/etc/hostapd/hostapd.conf\"" >> /etc/default/hostapd
fi

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common
setSetting rebootFlag 1
echo "A reboot will be required to get the tethering packages working"

