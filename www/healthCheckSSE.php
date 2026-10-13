<?
header("Content-Type: text/event-stream");
header("Cache-Control: no-cache");
header("X-Accel-Buffering: no");

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

$timestamp = isset($_GET['timestamp']) ? intval($_GET['timestamp']) : 0;

system($settings["fppDir"] . "/scripts/healthCheckSSE " . $timestamp);
?>
