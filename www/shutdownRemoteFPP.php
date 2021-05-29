<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "ERROR: No IP given\n";
    exit(0);
}

$ip = $_GET['ip'];

if(! filter_var($ip, FILTER_VALIDATE_IP)) {
    echo "$ip is not a valid IP address\n";
    exit(0);
}

echo "Shutting down FPP system @ $ip\n";

$curl = curl_init('http://' . $ip . '/fppxml.php?command=shutdownPi');
curl_setopt($curl, CURLOPT_FAILONERROR, true);
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
$request_content = curl_exec($curl);
curl_close($curl);

echo "\nSystem Shutdown";

?>
