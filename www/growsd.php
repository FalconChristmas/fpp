
<?php
$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();

if ($settings['Platform'] == "BeagleBone Black") {
    $command = "sudo /opt/fpp/SD/BBB-grow_partition.sh 2>&1";
} else if ($settings['Platform'] == "Raspberry Pi") {
    $command = "sudo /usr/bin/raspi-config --expand-rootfs";
}

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";

?>


