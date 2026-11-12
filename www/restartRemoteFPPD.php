<?php

$skipJSsettings = 1;
require_once "common.php";

if (!isset($_GET['ip'])) {
    http_response_code(400);
    echo "ERROR: No IP given\n";
    exit(0);
}
$ip = $_GET['ip'];

// Any LAN/VPN peer or (mDNS) name, never this device (see IsRemotePeerHost()).
if (!IsRemotePeerHost($ip)) {
    http_response_code(400);
    echo "ERROR: Invalid IP '" . htmlspecialchars((string) $ip, ENT_QUOTES, 'UTF-8') . "'\n";
    exit(0);
}

if (isset($_GET['mode'])) {
    $mode = $_GET['mode'];
    $curl = curl_init('http://' . $ip . '/api/settings/fppMode');
    RemotePeerPinCurl($curl, 'http://' . $ip . '/api/settings/fppMode');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, false); // a redirect could point back at loopback
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 300);
    curl_setopt($curl, CURLOPT_TIMEOUT, 15);
    curl_setopt($curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_setopt($curl, CURLOPT_POSTFIELDS, $mode);

    curl_exec($curl);
    $modeRc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
    curl_close($curl);
    if ($modeRc < 200 || $modeRc >= 300) {
        http_response_code(502);
        echo "ERROR: " . htmlspecialchars($ip) . " did not take the FPPD mode " . htmlspecialchars($mode) .
            ($modeRc ? " (HTTP " . intval($modeRc) . ")" : " (no answer)") . "; not restarting it\n";
        exit(0);
    }
}

// FPP 5.0+.  quick=1: a remote's RestartFPPD() has only ever taken 1, so the
// quick=true this sent got every remote a full stop and start.
RemotePlayerAction('http://' . $ip . '/api/system/fppd/restart?quick=1', 200, $rc, $connected);
// Before any output, so a refusal answers an HTTP error (the row shows failed).
$err = RemotePlayerActionError($ip, 'restart', $rc, $connected);
if ($err != '') {
    http_response_code(502);
    echo $err . (isset($_GET['mode']) ? "Its FPPD mode was set to " . htmlspecialchars($_GET['mode']) . "; restart it to apply.\n" : "");
    exit(0);
}

DisableOutputBuffering();

if (isset($_GET['mode'])) {
    echo "Setting FPPD mode @ " . htmlspecialchars($ip) . " to " . htmlspecialchars($_GET['mode']) . "\n";
}
echo "Restarting FPPD @ " . htmlspecialchars($ip) . "\n";
if ($rc == 0) {
    echo "No reply from " . htmlspecialchars($ip) . " to the restart; waiting for it anyway.\n";
}

echo "Waiting for fppd to come back up:\n";

$request_content = false;
$maxWait = 60; // max wait time in seconds
$endTime = time() + $maxWait; // already waited $x seconds

while ((time() < $endTime) && ($request_content === false)) {
    echo '.';
    $curl = curl_init('http://' . $ip . '/api/fppd/status');
    RemotePeerPinCurl($curl, 'http://' . $ip . '/api/fppd/status');
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
