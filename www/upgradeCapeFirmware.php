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
} else if (isset($_POST['filename']) || isset($_GET['filename'])) {
    $unlink = 0;

    $fn = isset($_GET['filename']) ? $_GET['filename'] : $_POST['filename'];

    if (preg_match('/^http/', $fn)) {
        $file = '/home/fpp/media/tmp/tmp-eeprom.bin';
        system("/usr/bin/wget -O $file " . $fn);
        $unlink = 1;
    } else if (preg_match('/\/opt\/fpp\/capes/', $fn)) {
        $file = $fn;

        if (file_exists('/home/fpp/media/config/cape-eeprom.bin')) {
            unlink('/home/fpp/media/config/co-bbbStrings.json');
            unlink('/home/fpp/media/config/co-pixelStrings.json');
        }

    } else {
        $file = $uploadDirectory . '/' . $fn;
    }
}

if ($file != '') {
    echo "Upgrading firmware.....\n";
    echo "\n";
    system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $force . $file, $retval);
    WriteSettingToFile('rebootFlag', 1);

    if ($unlink)
        unlink($file);

    echo "\n";
} else {
    echo "ERROR: No firmware file specified.";
}
