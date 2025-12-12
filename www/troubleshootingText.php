<?php
//stop settings javascript output in results
$skipJSsettings =1;

require_once 'common.php';

putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");

// Set up platform-specific variables (from troubleshootingHelper.php)
$rtcDevice = "/dev/rtc0";
$i2cDevice = "1";

if ($settings['BeaglePlatform']) {
    if (file_exists("/sys/class/rtc/rtc0/name")) {
        $rtcname = file_get_contents("/sys/class/rtc/rtc0/name");
        if (strpos($rtcname, "omap_rtc") !== false) {
            $rtcDevice = "/dev/rtc1";
        }
    }
    $i2cDevice = "2";
} else if ($settings['Platform'] == "Raspberry Pi") {
    if (file_exists("/sys/class/rtc/rtc0/name")) {
        $drv = file_get_contents("/sys/class/rtc/rtc0/name");
        if (str_contains($drv, ":rpi_rtc") && file_exists("/dev/rtc1")) {
            // Raspberry Pi 5 RTC, we are configuring the cape clock which would be rtc1
            $rtcDevice = "/dev/rtc1";
        }
    }
}

//LoadCommands
$troubleshootingCommandsLoaded = 0;
LoadTroubleShootingCommands();
$target_platforms = array('all', $settings['Platform']);


echo "Troubleshooting Commands:\n";

////Display Command Contents
foreach ($troubleshootingCommandGroups as $commandGrpID => $commandGrp) {
    //Loop through groupings
    //Display group if relevant for current platform
    if (count(array_intersect($troubleshootingCommandGroups[$commandGrpID]["platforms"], $target_platforms)) > 0) {
        echo "===================================================================\n";
        echo "Command Group: $commandGrpID\n";

    }
    //Loop through commands in grp
    foreach ($commandGrp["commands"] as $commandKey => $commandID) {
        //Display command if relevant for current platform
        if (count(array_intersect($commandID["platforms"], $target_platforms)) > 0) {
            $commandTitle = $commandID["title"];
            $commandCmd = $commandID["cmd"];
            $commandDesc = $commandID["description"];

            // Execute command directly instead of making HTTP request
            // Substitute PHP variables denoted by [[ variable name (without $) ]]
            preg_match_all('/\[\[(.*?)\]\]/', $commandCmd, $matches);
            foreach ($matches[1] as $value) {
                $commandCmd = str_replace('[[' . $value . ']]', ${$value}, $commandCmd);
            }
            
            exec($SUDO . ' ' . "/bin/sh -c '" . $commandCmd . "' 2>&1 | fold -w 160 -s", $output, $return_val);
            if ($return_val == 0) {
                $results = implode("\n", $output) . "\n";
            } else {
                $results = "Failed to return valid result\n";
            }
            unset($output);

    echo "Title  : $commandTitle\n";
    echo "Command: $commandCmd\n";
    echo "Command Description: $commandDesc\n";
    echo "-----------------------------------\n";
    echo $results;
    echo "\n";
    $results="";

        }}


}

echo "===================================================================\n";
