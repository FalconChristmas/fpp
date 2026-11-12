<?php

$skipJSsettings = 1;
require_once "common.php";

if (!isset($_GET['ip'])) {
    http_response_code(400);
    echo "ERROR: No IP given\n";
    exit(0);
}

$ip = $_GET['ip'];

if (!filter_var($ip, FILTER_VALIDATE_IP) || !IsRemotePeerHost($ip)) {
    $clean_ip = htmlspecialchars((string) $ip, ENT_QUOTES, 'UTF-8');
    http_response_code(400);
    echo "$clean_ip is not a valid IP address\n";
    exit(0);
}

// FPP 5.0+
RemotePlayerAction('http://' . $ip . '/api/system/shutdown', 200, $rc, $connected);
// Before any output, so a refusal answers an HTTP error (the row shows failed).
$err = RemotePlayerActionError($ip, 'shutdown', $rc, $connected);
if ($err != '') {
    http_response_code(502);
    echo $err;
    exit(0);
}

DisableOutputBuffering();

echo "Shutting down FPP system @ $ip\n";
if ($rc == 0) {
    echo "\nNo reply from $ip; it may already be shutting down.";
    exit(0);
}

echo "\nSystem Shutdown";
