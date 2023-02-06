<?php
require_once 'config.php';
require_once 'common.php';

putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");

$rtcDevice = "/dev/rtc0";
$i2cDevice = "1";

if ($settings['Platform'] == "BeagleBone Black") {
    if (file_exists("/sys/class/rtc/rtc0/name")) {
        $rtcname = file_get_contents("/sys/class/rtc/rtc0/name");
        if (strpos($rtcname, "omap_rtc") !== false) {
            $rtcDevice = "/dev/rtc1";
        }
    }
    $i2cDevice = "2";
}

$commands = array(
    // Networking
    'Interfaces' => 'ifconfig -a',
    'Wired' => 'ethtool eth0',
    'Wireless' => '(iwconfig ; echo ; echo ; cat /proc/net/wireless)',
    'Routing' => 'netstat -rn',
    'Default Gateway' => 'ping -c 1 $(netstat -rn | grep \'^0.0.0.0\' | awk \'{print $2}\')',
    'Internet Access' => 'curl -S -s -o /dev/null -m 2 https://github.com/FalconChristmas/fpp/blob/master/README.md 2>&1 ; if [ "$?" -eq "0" ]; then echo "GitHub reachable: **YES**" ; else echo "GitHub Reachable: ** NO **"; fi',

    // Disk
    'Block Devices' => $SUDO . ' lsblk -l',
    'Partitions' => $SUDO . ' fdisk -l',
    'Filesystems' => 'df -k',
    'Mounts' => 'mount | grep -v password',

    // Date/Time
    'Date' => 'date',
    'NTP Peers' => 'pgrep ntpd > /dev/null && ntpq -c peers',
    'RTC' => $SUDO . " hwclock -r -f $rtcDevice",

    // Memory & CPU
    'Memory' => 'free',
    'Uptime' => 'uptime',
    'CPU Utilization' => 'top -bn1 | head -20',
    'CPUInfo' => 'cat /proc/cpuinfo',

    // USB Devices
    'USB Device Tree' => $SUDO . ' lsusb -t',
    'USB Devices' => $SUDO . ' lsusb -v',

    // Audio
    'Sound Cards' => $SUDO . ' aplay -l',
    'Mixer Devices' => '(/bin/ls -1d /proc/asound/card[0-9] | sed -e "s/.*\/card//" | while read ID; do echo "CardID: ${ID}"; ' . $SUDO . ' amixer -c ${ID} ; echo ; done)',

    // Midi
    'Midi Devices' => 'aseqdump -l',

    // Video
    'Video' => 'test -e /dev/fb0 && echo "/dev/fb0" && fbset -s -fb /dev/fb0 ; test -e /dev/fb1 && echo "/dev/fb1" && fbset -s -fb /dev/fb1',

    // OS, Kernel, and SD image
    'OS Version' => 'test -e /etc/os-release && cat /etc/os-release',
    'Kernel Version' => 'uname -a',
    'Kernel Modules' => 'lsmod',
    'Image Version' => 'cat /etc/fpp/rfs_version',
    'Image Platform' => 'cat /etc/fpp/platform',
    'Image Config Version' => 'cat /etc/fpp/config_version',

    // i2c
    'i2cdetect' => $SUDO . ' i2cdetect -y -r ' . $i2cDevice,

    // Processes
    'Processes' => 'ps -edaf --forest', // Keep this last since it is so long

    // Boot
    'FPP Cape Detect Log' => $SUDO . ' journalctl -u fppcapedetect | tail -20 ',
    'FPP RTC Log' => $SUDO . ' journalctl -u fpprtc | tail -20 ',
    'FPP Init Log' => $SUDO . ' journalctl -u fppinit | tail -20 ',
    'FPP Post Network Logs' => $SUDO . ' journalctl -u fpp_postnetwork | tail -20 ',
    'FPP OLED Logs' => $SUDO . ' journalctl -u fppoled | tail -20 ',
    'FPP FPPD Logs' => $SUDO . ' journalctl -u fppd | tail -20 ',
);

if ($settings['Platform'] != "MacOS") {
        
    // GPIO Detect
    $commands['GPIO'] = $SUDO . ' gpiodetect ';

    // GPIO Info
    $commands['GPIO Info'] = $SUDO . ' gpioinfo ';
    }
