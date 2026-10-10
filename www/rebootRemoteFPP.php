<?php

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}

$ip = $_GET['ip'];

// Validate IP/hostname (prevents SSRF, matches streamRemote.php / changeRemoteBranch.php).
// Intentionally syntax-only, not resolvability: targets are LAN peers by design
// (including mDNS names), so private addresses must keep working.
if (!is_string($ip)) {
    echo "ERROR: Invalid IP given\n";
    exit(0);
}
$validIp = filter_var($ip, FILTER_VALIDATE_IP);
$validHost = preg_match('/^(?=.{1,253}$)([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?$/', $ip);
if (!$validIp && !$validHost) {
    echo "ERROR: Invalid IP '" . htmlspecialchars($ip, ENT_QUOTES, 'UTF-8') . "'\n";
    exit(0);
}

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
