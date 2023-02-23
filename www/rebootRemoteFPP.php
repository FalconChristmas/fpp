<?php

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}

$ip = $_GET['ip'];

echo "Rebooting FPP system @ " . htmlspecialchars($ip) . "\n";

// FPP 5.0+
$curl = curl_init('http://' . $ip . '/api/system/reboot');
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
$request_content = curl_exec($curl);
$rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);

echo "Waiting for system to come back up:\n";
for ($x = 0; $x < 20; $x++) {
    echo '.';
    sleep(1);
}

$request_content = false;
$maxWait = 180; // max wait time in seconds
$endTime = time() + $maxWait - $x; // already waited $x seconds

while ((time() < $endTime) && ($request_content === false)) {
    echo '.';
    $curl = curl_init('http://' . $ip . '/api/fppd/status');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    curl_close($curl);

    if ($request_content === false) {
        sleep(1);
    }
}

if ($request_content === false) {
    echo "\nError, timed out waiting for system to reboot!  Waited $maxWait seconds.\n";
} else {
    echo "\nSystem Rebooted";
}
