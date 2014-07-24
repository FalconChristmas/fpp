#!/bin/sh
#
# fppLCD install/upgrade script
#
# Recreate /etc/network/interfaces to switch eth0 to allow-hotplug
#

cat <<-EOF > /etc/network/interfaces
	auto lo

	allow-hotplug eth0

	iface lo inet loopback

	iface eth0 inet dhcp

	iface wlan0 inet dhcp
	  wpa-roam /etc/wpa_supplicant/wpa_supplicant.conf

	auto eth0:0
	iface eth0:0 inet static
	  address 192.168.0.10
	  netmask 255.255.255.0
	  network 192.168.0.0
	EOF

