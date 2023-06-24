<?php

header( "Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();


        echo "==================================================================================\n";
        
        $command = "sudo TERM=vt100 env 2>&1";
        
        echo "Command: $command\n";
        echo "----------------------------------------------------------------------------------\n";
        system($command);
    
		echo "==================================================================================\n";

		$command = "sudo TERM=vt100 /opt/fpp/SD/BBB-FlashMMC.sh btrfs 2>&1";

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";

echo "----------------------------------------------------------------------------------\n";
echo "\n";
echo "\n";
?>
