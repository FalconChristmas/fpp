#!/bin/bash
#
# Download and install new Edimax wireless module on BBB
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'BeagleBone Black')
		echo "FPP - Installing updated 8192cu module"
		wget -O /lib/modules/3.8.13-bone50/kernel/drivers/net/wireless/8192cu.ko https://github.com/FalconChristmas/fpp/releases/download/1.5/8192cu.ko

		echo "FPP - Disabling power management for 8192cu wireless"
cat <<-EOF > /etc/modprobe.d/8192cu.conf
		# Blacklist native RealTek 8188CUs drivers
		blacklist rtl8192cu
		blacklist rtl8192c_common
		blacklist rtlwifi

		# Disable power management on 8192cu module
		options 8192cu rtw_power_mgnt=0 rtw_enusbss=0
EOF

	;;
esac


