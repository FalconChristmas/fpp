<?php

$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();

    $command = "sudo /opt/fpp/SD/newpartition.sh 2>&1";

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";

?>
