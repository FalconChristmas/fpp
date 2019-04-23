#!/bin/bash
#####################################
# copy the oled service definition to systemd and turn the service
# on a boot.  We won't set the reboot flag as the default is disabled
# and once the user selects an OLED type, it will set the reboot flag

cp -f ${FPPDIR}/etc/systemd/*.service /lib/systemd/system
systemctl disable fppoled
systemctl disable fppinit
systemctl enable fppoled
systemctl enable fpprtc
systemctl enable fppcapedetect
systemctl enable fppinit

# Install libgpiod and gpiod for the pushbutton support
apt-get -y update
apt-get -y install gpiod libgpiod-dev
apt-get -y install zstd libzstd-dev libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat
apt-get -y --reinstall install libboost-filesystem-dev libboost-system-dev libboost-iostreams-dev

# Cleanup the apt stuff to free up space
apt-get remove -y --autoremove libmagickcore-6.q16-3 libmagickcore-6.q16-3-extra libmagickwand-6.q16-3
apt-get clean
apt autoremove

# Update the boot scripts on the BBB
FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)
case "${FPPPLATFORM}" in
    'BeagleBone Black')
        cd /opt/scripts
        git reset --hard
        git pull
        sed -i 's/systemctl restart serial-getty/systemctl is-enabled serial-getty/g' boot/am335x_evm.sh
    ;;
esac
