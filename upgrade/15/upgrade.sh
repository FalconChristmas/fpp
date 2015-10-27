#!/bin/bash
#
# Download and install new Edimax wireless module on BBB
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'BeagleBone Black')
		if [ ! -s /lib/modules/3.8.13-bone50/kernel/drivers/net/wireless/8192cu.ko ]
		then
			echo "FPP - Re-downloading updated 8192cu module"
			wget -O /lib/modules/3.8.13-bone50/kernel/drivers/net/wireless/8192cu.ko https://github.com/FalconChristmas/fpp/releases/download/1.5/8192cu.ko
			depmod -a
			modprobe 8192cu
		fi
	;;
esac


