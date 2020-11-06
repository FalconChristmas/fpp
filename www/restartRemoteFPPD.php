<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}
$ip = $_GET['ip'];

$postfix = '';
if (isset($_GET['mode'])) {
    echo "Setting FPPD mode @ $ip\n";
    $mode = $_GET['mode'];
    $curl = curl_init('http://' . $ip . '/fppxml.php?command=setFPPDmode&mode=' . $mode);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 300);
    $request_content = curl_exec($curl);
    curl_close($curl);
    $postfix = '&quick=1';
}

echo "Restarting FPPD @ $ip\n";

$curl = curl_init('http://' . $ip . '/fppxml.php?command=restartFPPD' . $postfix);
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 300);
$request_content = curl_exec($curl);
curl_close($curl);

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

    sleep(1);
}

if ($request_content === FALSE) {
    echo "\nError, timed out waiting for fppd to restart  Waited $maxWait seconds.\n";
} else {
    echo "\nFPPD Restarted";
}

?>
