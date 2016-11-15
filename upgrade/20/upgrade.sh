#!/bin/bash
#
# Update the Raspberry Pi to allow DMX using the onboard serial port
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'Raspberry Pi')
		echo >> /boot/config.txt

		echo "# Configure onboard serial to allow DMX" >> /boot/config.txt
		echo "init_uart_clock=16000000" >> /boot/config.txt
		echo >> /boot/config.txt

		echo "# Switch Pi v3 onboard serial and Bluetooth to allow DMX on Pi v3" >> /boot/config.txt
		echo "dtoverlay=pi3-miniuart-bt" >> /boot/config.txt
		echo >> /boot/config.txt
	;;
esac


