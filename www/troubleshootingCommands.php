<?php
require_once('config.php');
require_once('common.php');

putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");

$rtcDevice = "/dev/rtc0";
if ($settings['Platform'] == "BeagleBone Black") {
    $rtcDevice = "/dev/rtc1";
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
    'CPU Utilization'    => 'top -bn1 | head -20',
    'CPUInfo'            => 'cat /proc/cpuinfo',

    // USB Devices
    'USB Device Tree'    => $SUDO . ' lsusb -t',
    'USB Devices'        => $SUDO . ' lsusb -v',

    // Audio
    'Sound Cards'        => $SUDO . ' aplay -l',
    'Mixer Devices'      => '(/bin/ls -1d /proc/asound/card[0-9] | sed -e "s/.*\/card//" | while read ID; do echo "CardID: ${ID}"; ' . $SUDO . ' amixer -c ${ID} ; echo ; done)',

    // GPIO
    'GPIO'               => $SUDO . ' gpio readall',

    // Kernel
    'Kernel Version'     => 'uname -a',
    'Kernel Modules'     => 'lsmod',

    // Processes
    'Processes'          => 'ps -edaf --forest',  // Keep this last since it is so long
    );

$results = array();

foreach ($commands as $title => $command) {
    $results[$command] = "Unknown";
    exec($command . ' 2>&1 | fold -w 80 -s', $output, $return_val);
    if ($return_val == 0) {
        $results[$command] = implode("\n", $output) . "\n";
    }
    unset($output);
}
