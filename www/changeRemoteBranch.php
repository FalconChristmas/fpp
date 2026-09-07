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
// Validate branch same as www/changebranch.php:26 (keeps PR 2910 intact)
if (!preg_match('/^[A-Za-z0-9_.\/-]+$/', $branch) || strpos($branch, '..') !== false || $branch === '' || $branch[0] === '-') {
    echo "ERROR: Invalid branch '" . htmlspecialchars($branch) . "'\n";
    flush();
    if (ob_get_level() > 0) ob_flush();
    exit(0);
}

$remote = isset($_GET['remote']) ? $_GET['remote'] : 'origin';
// Validate remote name to prevent injection
if (!preg_match('/^[a-zA-Z0-9_-]+$/', $remote)) {
    $remote = 'origin';
}

// Validate IP/hostname (prevents SSRF, matches streamRemote.php:31 / remotePush.php:55)
$validIp = filter_var($ip, FILTER_VALIDATE_IP);
$validHost = preg_match('/^(?=.{1,253}$)([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?$/', $ip);
if (!$validIp && !$validHost) {
    echo "ERROR: Invalid IP '" . htmlspecialchars($ip) . "'\n";
    flush();
    if (ob_get_level() > 0) ob_flush();
    exit(0);
}

echo "Changing branch @" . htmlspecialchars($ip) . " to " . htmlspecialchars($branch) . " (remote: " . htmlspecialchars($remote) . ")\n";
flush();
if (ob_get_level() > 0) ob_flush();

$curl = curl_init('http://' . $ip . '/api/system/fppd/stop');
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
curl_exec($curl);
curl_close($curl);


$curl = curl_init('http://' . $ip . '/changebranch.php?branch=' . urlencode($branch) . '&remote=' . urlencode($remote));
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
// Do not use FAILONERROR so 400 Invalid branch body is streamed to log window (PR 2910 compat)
curl_exec($curl);
$httpCode = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);
if ($httpCode >= 400) {
    echo "\nRemote returned HTTP $httpCode - check branch name\n";
    flush();
    if (ob_get_level() > 0) ob_flush();
}

echo "Waiting for fppd to come back up:\n";
flush();
if (ob_get_level() > 0) ob_flush();
sleep(2);

$request_content = false;
$maxWait = 60; // max wait time in seconds
$endTime = time() + $maxWait; // already waited $x seconds

$count = 0;
while ((time() < $endTime) && ($request_content === false)) {
    echo '.';
    flush();
    if (ob_get_level() > 0) ob_flush();
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