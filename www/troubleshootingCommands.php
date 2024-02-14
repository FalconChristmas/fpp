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
} else if (strpos($settings['SubPlatform'], "Raspberry Pi 5") !== false) {
    // rtc0 is the onboard RTC, if rtc1 exists, that's the clock on the cape
    if (file_exists("/dev/rtc1")) {
        $rtcDevice = "/dev/rtc1";
    }
}

if ($settings['Platform'] != "MacOS") {
    //GPIO Settings
    $poss_paths = explode(":", getenv("PATH"));
    $gpio_present = 0;
    foreach ($poss_paths as $path) {
        if (file_exists($path . "/gpiodetect") && $gpio_present == 0) {
            $gpio_present = 1;
        }
    }
    if ($gpio_present == 1) {
        // GPIO Detect
        $commands['GPIO'] = $SUDO . ' gpiodetect ';

        // GPIO Info
        $commands['GPIO Info'] = $SUDO . ' gpioinfo ';
    }
}
