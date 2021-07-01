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
    $clean_ip = htmlspecialchars($ip, ENT_QUOTES, 'UTF-8');
    echo "$clean_ip is not a valid IP address\n";
    exit(0);
}

echo "Shutting down FPP system @ $ip\n";


// FPP 5.0+
$curl = curl_init('http://' . $ip . '/api/system/shutdown');
curl_setopt($curl, CURLOPT_FAILONERROR, false);  // don't fail hard if not 5.0+
curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
$request_content = curl_exec($curl);
$rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);

// If 5.0+ method failed, try old method.
if (! $request_content || $rc != 200 ) {
    $curl = curl_init('http://' . $ip . '/fppxml.php?command=shutdownPi');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    curl_close($curl);
}


echo "\nSystem Shutdown";

?>
