#!/bin/bash
#
# Update the Raspberry Pi to allow maximum current through the USB
# ports for people to operate certain things without a hub required.
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'Raspberry Pi')
		echo "# Allow more current through USB" >> /boot/config.txt
		echo "max_usb_current=1" >> /boot/config.txt
		echo >> /boot/config.txt
	;;
esac


