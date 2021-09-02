<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}
$ip = $_GET['ip'];

if (isset($_GET['mode'])) {
    echo "Setting FPPD mode @ ".htmlspecialchars($ip)."\n";
    $mode = $_GET['mode'];
    $curl = curl_init('http://' . $ip . '/fppxml.php?command=setFPPDmode&mode=' . $mode);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 300);
    $request_content = curl_exec($curl);
    curl_close($curl);
}

echo "Restarting FPPD @ ".htmlspecialchars($ip)."\n";

// FPP 5.0+
$curl = curl_init('http://' . $ip . '/api/system/fppd/restart?quick=true');
curl_setopt($curl, CURLOPT_FAILONERROR, false);  // don't fail hard if not 5.0+
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
$request_content = curl_exec($curl);
$rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);

// If 5.0+ method failed, try old method.
if (! $request_content || $rc != 200 ) {
    $curl = curl_init('http://' . $ip . '/fppxml.php?command=restartFPPD');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    $rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
    curl_close($curl);
}

echo "Waiting for fppd to come back up:\n";

$request_content = FALSE;
$maxWait = 60;                // max wait time in seconds
$endTime = time() + $maxWait; // already waited $x seconds

while ((time() < $endTime) && ($request_content === FALSE)) {
    echo '.';
    $curl = curl_init('http://' . $ip . '/api/fppd/status');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    curl_close($curl);

    if ($request_content === FALSE) {
        sleep(1);
    }
}

if ($request_content === FALSE) {
    echo "\nError, timed out waiting for fppd to restart  Waited $maxWait seconds.\n";
} else {
    echo "\nFPPD Restarted";
}

?>
