<?php

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}
$ip = $_GET['ip'];

// Any LAN/VPN peer or (mDNS) name, never this device (see IsRemotePeerHost()).
if (!IsRemotePeerHost($ip)) {
    echo "ERROR: Invalid IP '" . htmlspecialchars((string) $ip, ENT_QUOTES, 'UTF-8') . "'\n";
    exit(0);
}

if (isset($_GET['mode'])) {
    echo "Setting FPPD mode @ " . htmlspecialchars($ip) . " to " . htmlspecialchars($_GET['mode']) . "\n";
    $mode = $_GET['mode'];
    $curl = curl_init('http://' . $ip . '/api/settings/fppMode');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, false); // a redirect could point back at loopback
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 300);
    curl_setopt($curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_setopt($curl, CURLOPT_POSTFIELDS, $mode);

    $request_content = curl_exec($curl);
    curl_close($curl);
}

echo "Restarting FPPD @ " . htmlspecialchars($ip) . "\n";

// FPP 5.0+
$request_content = RemotePlayerAction('http://' . $ip . '/api/system/fppd/restart?quick=true', 200, $rc);

echo "Waiting for fppd to come back up:\n";

$request_content = false;
$maxWait = 60; // max wait time in seconds
$endTime = time() + $maxWait; // already waited $x seconds

while ((time() < $endTime) && ($request_content === false)) {
    echo '.';
    $curl = curl_init('http://' . $ip . '/api/fppd/status');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, false); // a redirect could point back at loopback
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
