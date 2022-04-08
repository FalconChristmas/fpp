<?php
$wrapped = 0;
$skipJSsettings = 1;
require_once 'common.php';
DisableOutputBuffering();

$force = "";
if (isset($_GET['force']) && $_GET['force'] == 'true') {
    $force = "force ";
}
$file = $_FILES["firmware"]["tmp_name"];

echo "Upgrading firmware.....\n";
echo "\n";
system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $force . $file, $retval);
WriteSettingToFile('rebootFlag', 1);
unlink($file);
echo "\n";
echo "Finished.  Please reboot.";
