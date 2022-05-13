<?php
$wrapped = 0;
$skipJSsettings = 1;
require_once 'common.php';
DisableOutputBuffering();

$force = "";
if (isset($_GET['force']) && $_GET['force'] == 'true') {
    $force = "force ";
}

$action = 'Upgrading';
$file = '';
$unlink = 1;
if (isset($_FILES['firmware']) && isset($_FILES['firmware']['tmp_name'])) {
    $file = $_FILES["firmware"]["tmp_name"];
} else if (isset($_GET['filename'])) {
    $file = $uploadDirectory . '/' . $_GET['filename'];
    $unlink = 0;
}

if ($file != '') {
    echo "Upgrading firmware.....\n";
    echo "\n";
    system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $force . $file, $retval);
    WriteSettingToFile('rebootFlag', 1);

    if ($unlink)
        unlink($file);

    echo "\n";
    echo "Finished.  Please reboot.";
} else {
    echo "ERROR: No firmware file specified.";
}
