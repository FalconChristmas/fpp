<?
//stop settings javascript output in results
$skipJSsettings = 1;
// This file runs each command when called via the specified key
// and returns the raw results

require_once 'config.php';
require_once 'common.php';

putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");

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
}

//Load commands from JSON
LoadTroubleShootingCommands();

foreach ($troubleshootingCommandGroups as $commandGrpID => $commandGrp) {

    foreach ($commandGrp["commands"] as $commandKey => $commandID) {
        $commands[$commandKey] = $commandID["cmd"];
    }
}

//Process command and return result base on command key passed in URL
$found = 0;
if (isset($_GET['key'])) {
    $key = $_GET['key'];
    if (isset($commands[$key])) {
        $found = 1;
        $command = $commands[$key];

        ////////// Substitute PHP variables denoted by [[ variable name (without $) ]] in command string to allow accessing normal settings variables and any specials declared above
        preg_match_all('/\[\[(.*?)\]\]/', $command, $matches);

        foreach ($matches[1] as $value) {
            $command = str_replace('[[' . $value . ']]', ${$value}, $command);
        }
        ////////

        exec($SUDO . ' ' . "/bin/sh -c '" . $command . "' 2>&1 | fold -w 160 -s", $output, $return_val);
        if ($return_val == 0) {
            echo (implode("\n", $output) . "\n");
        } else {
            echo "Failed to return valid result";
        }

        unset($output);
    }
}

if (!$found) {
    echo ("Unknwon Command");
}

?>