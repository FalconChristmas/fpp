#!/bin/bash

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)
FPPOSVERSION=$(cat /etc/fpp/rfs_version 2> /dev/null)
# copy updated service definitions
cp -a /opt/fpp/etc/systemd/fpp*.service /lib/systemd/system

#On the BBB, the v2.2 image had some chages from TexasInstruments that
#Significanly slows down the boot process (20-30 seconds)
#We'll update and "fix" that issue.  The "restart" of serial-getty is
#taking a ton of time, but we have it "enabled" so it will start anyway
#so we'll just call is-enabled to avoid the restart thing
if [ "v2.2" == "$FPPOSVERSION" ]; then
case "${FPPPLATFORM}" in
    'BeagleBone Black')
        cd /opt/scripts
        git reset --hard
        git pull
        sed -i 's/systemctl restart serial-getty/systemctl is-enabled serial-getty/g' boot/am335x_evm.sh
    ;;
esac
fi
