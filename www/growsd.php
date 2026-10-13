<?php
$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();
// Finish the job if the browser goes away: a closed tab or dropped connection
// used to abort PHP part way through, leaving the command it ran half done.
ignore_user_abort(true);

$command2 = "";
if ($settings['Platform'] == "BeagleBone Black") {
    if (file_exists("/usr/bin/bb-growpart")) {
        $command = "sudo /usr/bin/bb-growpart 2>&1";
        $command2 = "sudo systemctl enable resize_filesystem.service";
    } else {
        $command = "sudo /opt/fpp/SD/BBB-grow_partition.sh 2>&1";
    }
} else if ($settings['Platform'] == "Raspberry Pi") {
    $command = "sudo /usr/bin/raspi-config --expand-rootfs";
}

echo "Command: $command\n";
echo "----------------------------------------------------------------------------------\n";
system($command);
if ($command2 != "") {
    system($command2);
}
echo "\n";
?>


