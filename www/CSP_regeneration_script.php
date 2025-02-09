<?php
$skipJSsettings = 1;
// This script regenerates the Content Security Policy (CSP) for Apache. It exists so that JS can call the URL and regenerate the CSP without needing to reload the page.
require_once "config.php";
require_once "common.php";


$script_path = $fppDir . "/scripts/ManageApacheContentPolicy.sh";

// Make sure the script has execute permissions
$output = shell_exec("sudo bash $script_path regenerate 2>&1");

//echo "<pre>$output</pre>";
?>