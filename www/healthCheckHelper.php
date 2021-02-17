<?
header( "Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

system("/opt/fpp/scripts/healthCheck --php");
?>
