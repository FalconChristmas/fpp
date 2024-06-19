<?
header("Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once ("common.php");

DisableOutputBuffering();

system($settings["fppDir"] . "/scripts/healthCheck --php " . $_GET['timestamp']);
?>