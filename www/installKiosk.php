<?
header( "Access-Control-Allow-Origin: *");

$wrapped = 1;
$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();

echo "==========================================================================\n";

$command = "sudo /opt/fpp/SD/FPP_Kiosk.sh 2>&1";

echo "Command: $command\n";
echo "--------------------------------------------------------------------------\n";
system($command);
echo "\n";

echo "==========================================================================\n";

?>
