#!/bin/bash

#add symlinks from our bin.pi or bin.bbb directories to the bins in /src
FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)
case "${FPPPLATFORM}" in
	'Raspberry Pi')
        #remove existing entries
        sudo rm -f /opt/fpp/bin.pi/fppmm
        sudo rm -f /opt/fpp/bin.pi/fppd
        sudo rm -f /opt/fpp/bin.pi/fpp
        #add symlinks
        sudo ln -s /opt/fpp/src/fpp /opt/fpp/bin.pi/
        sudo ln -s /opt/fpp/src/fppd /opt/fpp/bin.pi/
        sudo ln -s /opt/fpp/src/fppmm /opt/fpp/bin.pi/
	;;
	'BeagleBone Black')
        #remove existing entries
        sudo rm -f /opt/fpp/bin.bbb/fppmm
        sudo rm -f /opt/fpp/bin.bbb/fppd
        sudo rm -f /opt/fpp/bin.bbb/fpp
        #add symlinks
        sudo ln -s /opt/fpp/src/fpp /opt/fpp/bin.bbb/
        sudo ln -s /opt/fpp/src/fppd /opt/fpp/bin.bbb/
        sudo ln -s /opt/fpp/src/fppmm /opt/fpp/bin.bbb/
	;;
esac