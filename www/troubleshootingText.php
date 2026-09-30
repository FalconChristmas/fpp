<?php
// Command line only (generate_crash_report); never served as a page
if (PHP_SAPI !== 'cli') {
    http_response_code(404);
    exit;
}

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

// --manual-crash-report: output is going into a crash report.  A command's
// "manualCrashReportCmd" in troubleshoot-commands.json runs instead where it has
// one.  A command marked "pii" prints free-form text the crash report's redactor
// cannot clean (SSIDs, command lines), so without one it is left out.
$crashReport = isset($argv) && in_array('--manual-crash-report', $argv, true);
if ($crashReport) {
    // generate_crash_report runs from media/, and the Git commands need the repo
    chdir(__DIR__);
}
// No timeout on macOS without coreutils: run commands unlimited there
$timeoutCmd = trim((string)shell_exec('command -v timeout 2>/dev/null'));

// Already root from a crash report: sudo would log each command to fppd's journal
if (function_exists('posix_geteuid') && posix_geteuid() == 0) {
    $SUDO = "";
}


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
            $omit = false;
            if ($crashReport) {
                if (!empty($commandID["manualCrashReportCmd"])) {
                    $commandCmd = $commandID["manualCrashReportCmd"];
                } else if (!empty($commandID["pii"])) {
                    $omit = true;
                }
            }

            // Execute command directly instead of making HTTP request
            // Substitute PHP variables denoted by [[ variable name (without $) ]]
            preg_match_all('/\[\[(.*?)\]\]/', $commandCmd, $matches);
            foreach ($matches[1] as $value) {
                $commandCmd = str_replace('[[' . $value . ']]', ${$value}, $commandCmd);
            }
            
            if ($omit) {
                $results = "** omitted from crash report (pii) **\n";
            } else {
                // A hung command costs 20s, not the whole report.  Not wrapped:
                // the redactor needs a secret and its key on one line.
                $started = microtime(true);
                exec($SUDO . ' ' . ($timeoutCmd !== '' ? "$timeoutCmd -k 2 20 " : '') . "/bin/sh -c '" . $commandCmd . "' 2>&1", $output, $return_val);
                $results = $output ? implode("\n", $output) . "\n" : "";
                // 124/137 from a command's own timeout (e.g. timeout 3 pw-cli) is
                // just an exit status
                if (($return_val == 124 || $return_val == 137) && microtime(true) - $started >= 20) {
                    $results .= "** timed out after 20s **\n";
                } else if ($return_val != 0) {
                    $results .= "(exit $return_val)\n";
                }
                unset($output);
            }

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
