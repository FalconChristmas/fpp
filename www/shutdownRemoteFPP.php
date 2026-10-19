<?php

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}

$ip = $_GET['ip'];

if (!filter_var($ip, FILTER_VALIDATE_IP) || !IsRemotePeerHost($ip)) {
    $clean_ip = htmlspecialchars((string) $ip, ENT_QUOTES, 'UTF-8');
    echo "$clean_ip is not a valid IP address\n";
    exit(0);
}

echo "Shutting down FPP system @ $ip\n";

// FPP 5.0+
$request_content = RemotePlayerAction('http://' . $ip . '/api/system/shutdown', 200, $rc);

echo "\nSystem Shutdown";
