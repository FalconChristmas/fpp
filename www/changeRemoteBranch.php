<?php

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}
$ip = $_GET['ip'];

if (!isset($_GET['branch'])) {
    echo "ERROR: No branch given\n";
    exit(0);
}
$branch = $_GET['branch'];

echo "Changing branch @" . htmlspecialchars($ip) . " to " . $branch . "\n";

$curl = curl_init('http://' . $ip . '/api/system/fppd/stop');
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
curl_exec($curl);
curl_close($curl);


$curl = curl_init('http://' . $ip . '/changebranch.php?branch=' . $branch);
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
curl_exec($curl);
curl_close($curl);

echo "Waiting for fppd to come back up:\n";
sleep(2);

$request_content = false;
$maxWait = 60; // max wait time in seconds
$endTime = time() + $maxWait; // already waited $x seconds

$count = 0;
while ((time() < $endTime) && ($request_content === false)) {
    echo '.';
    if ($count == 14) {
        $curl = curl_init('http://' . $ip . '/api/system/fppd/restart');
        curl_setopt($curl, CURLOPT_FAILONERROR, true);
        curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
        curl_exec($curl);
        curl_close($curl);
        $count = 0;
    }
    $count = $count + 1;

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
    echo "\nError, timed out waiting for fppd to restart  Waited $maxWait seconds.\n";
} else {
    echo "\nFPPD Restarted";
}


?>