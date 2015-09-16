#!/bin/bash
#
# - Add fpp user to spi and video groups to match FPP v1.0 Pi image
# - Fix Edimax wireless power management on the BBB
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

adduser fpp spi
adduser fpp video

case "${FPPPLATFORM}" in
	'BeagleBone Black')
		echo -e "# Disable power management\noptions rtl8192cu rtw_power_mgnt=0 rtw_enusbss=0" > /etc/modprobe.d/rtl8192cu.conf

	;;
esac

