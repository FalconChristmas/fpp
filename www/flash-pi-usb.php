<?
header( "Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();


$device = $_GET['dev'];
$clone = $_GET['clone'];
$cloneFlag = "";
if ($clone == 'true') {
    $cloneFlag = "-clone";
}

        print("==================================================================================\n");
        
        $command = "sudo TERM=vt100 env 2>&1";
        
        print("Command: $command\n");
        print("----------------------------------------------------------------------------------\n");
        system($command);
    
		echo "==================================================================================\n";

        $command = "sudo TERM=vt100 /opt/fpp/SD/Pi-FlashUSB.sh $cloneFlag $device 2>&1";

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";
		echo "\n";
		echo "----------------------------------------------------------------------------------\n";
if (file_exists("/boot/recovery.bin")) {
    WriteSettingToFile("rebootFlag", "1");
}
?>