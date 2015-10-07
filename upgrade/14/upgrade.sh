#!/bin/bash
#
# Turn off forced HDMI output on the Pi so composite works
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'Raspberry Pi')
		sed -i -e "s/hdmi_force_hotplug/#hdmi_force_hotplug/" /boot/config.txt
	;;
esac


