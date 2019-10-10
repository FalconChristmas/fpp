#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

cp /opt/fpp/etc/systemd/fppcapedetect.service /lib/systemd/system
cp /opt/fpp/etc/update-RTC /etc/cron.daily

systemctl daemon-reload
systemctl reenable fppcapedetect

setSetting rebootFlag 1
echo "A reboot will be required to get the cape detect running after media mount"
