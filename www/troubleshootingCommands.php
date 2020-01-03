<?php
require_once('config.php');
require_once('common.php');

putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");

$rtcDevice = "/dev/rtc0";
$i2cDevice = "1";

if ($settings['Platform'] == "BeagleBone Black")
{
	$rtcDevice = "/dev/rtc1";
	$i2cDevice = "2";
}

$commands = array(
	// Networking
	'Interfaces'         => 'ifconfig -a',
	'Wired'              => 'ethtool eth0',
	'Wireless'           => 'iwconfig',
	'Routing'            => 'netstat -rn',
	'Default Gateway'    => 'ping -c 1 $(netstat -rn | grep \'^0.0.0.0\' | awk \'{print $2}\')',
	'Internet Access'    => 'ping -c 1 github.com',

	// Disk
	'Block Devices'      => $SUDO . ' lsblk -l',
	'Partitions'         => $SUDO . ' fdisk -l',
	'Filesystems'        => 'df -k',
	'Mounts'             => 'mount | grep -v password',

	// Date/Time
	'Date'               => 'date',
	'NTP Peers'          => 'pgrep ntpd > /dev/null && ntpq -c peers',
	'RTC'                => $SUDO . " hwclock -r -f $rtcDevice",

	// Memory & CPU
	'Memory'             => 'free',
	'Uptime'             => 'uptime',
	'CPU Utilization'    => 'top -bn1 | head -20',
	'CPUInfo'            => 'cat /proc/cpuinfo',

	// USB Devices
	'USB Device Tree'    => $SUDO . ' lsusb -t',
	'USB Devices'        => $SUDO . ' lsusb -v',

	// Audio
	'Sound Cards'        => $SUDO . ' aplay -l',
	'Mixer Devices'      => '(/bin/ls -1d /proc/asound/card[0-9] | sed -e "s/.*\/card//" | while read ID; do echo "CardID: ${ID}"; ' . $SUDO . ' amixer -c ${ID} ; echo ; done)',

	// Kernel
	'Kernel Version'     => 'uname -a',
	'Kernel Modules'     => 'lsmod',

	// i2c
	'i2cdetect'          => $SUDO . ' i2cdetect -y -r ' . $i2cDevice,

	// Processes
	'Processes'          => 'ps -edaf --forest',  // Keep this last since it is so long

        // Boot
        'FPP Init Log'          => $SUDO . ' journalctl -u fppinit ',
        'FPP Cape Detect Log'   => $SUDO . ' journalctl -u fppcapedetect ',
        'FPP Post Network Logs' => $SUDO . ' journalctl -u fpp_postnetwork ',
        'FPP OLED Logs' => $SUDO . ' journalctl -u fppoled ',
	);

$results = array();

foreach ($commands as $title => $command)
{
	$results[$command] = "Unknown";
	exec($command . ' 2>&1 | fold -w 80 -s', $output, $return_val);
	if ( $return_val == 0 )
		$results[$command] = implode("\n", $output) . "\n";
	unset($output);
}

?>
