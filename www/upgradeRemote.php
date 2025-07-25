<?php

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}

$ip = $_GET['ip'];

if (!filter_var($ip, FILTER_VALIDATE_IP)) {
    $clean_ip = htmlspecialchars($ip, ENT_QUOTES, 'UTF-8');
    echo "$clean_ip is not a valid IP address\n";
    exit(0);
}

echo "Upgrading FPP @ $ip\n";

// FPP 5.0+
$curl = curl_init('http://' . $ip . '/manualUpdate.php?wrapped=1');
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
curl_exec($curl);
$rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);

echo "\nFPP Upgraded\n";
