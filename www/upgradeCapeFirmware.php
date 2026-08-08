<?php
$wrapped = 0;
$skipJSsettings = 1;
require_once 'common.php';
DisableOutputBuffering();

$force = "";
if (isset($_GET['force']) && $_GET['force'] == 'true') {
    $force = "force ";
}

$resetDefaults = "";
if (isset($_GET['resetDefaults']) && $_GET['resetDefaults'] == 'true') {
    $resetDefaults = "resetDefaults ";
} else if (isset($_POST['resetDefaults']) && $_POST['resetDefaults'] == 'true') {
    $resetDefaults = "resetDefaults ";
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
        echo "Downloading $fn to $file \n\n";
        system("/usr/bin/wget -O " . escapeshellarg($file) . " " . escapeshellarg($fn) . " 2>&1");
        if (!file_exists($file) || filesize($file) < 72) {
            echo "\n\nProblems downloading firmware.  Check above errors for details.\n";
            echo "You may be able to manually download the file and do the upgrade directly with the file.\n";
            unlink($file);
            $file = '';
        }
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
    flush();
    // Wrap the file path as a single shell arg so the filename/URL cannot inject
    // additional commands. $force/$resetDefaults are fixed constant strings.
    system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $force . $resetDefaults . escapeshellarg($file), $retval);
    WriteSettingToFile('rebootFlag', 1);
    flush();

    if ($unlink && file_exists($file)) {
        unlink($file);
    }

    echo "\n";
} else {
    echo "\nERROR: No firmware file specified.\n";
}
