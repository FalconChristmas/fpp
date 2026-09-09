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

    // Zero-risk traversal block: reject .. and null bytes without allow-list
    if (strpos($fn, '..') !== false || strpos($fn, "\0") !== false) {
        echo "Invalid filename: traversal not allowed\n";
        $file = '';
    } else if (preg_match('/^http/', $fn)) {
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
        // Ensure the resolved path stays under /opt/fpp/capes
        $realBase = realpath('/opt/fpp/capes');
        $realPath = realpath($fn);
        // realpath returns false if file doesn't exist yet — fall back to string prefix check
        if ($realPath) {
            if (!$realBase || strpos($realPath, $realBase) !== 0) {
                echo "Invalid cape file path\n";
                $file = '';
            } else {
                $file = $fn;
            }
        } else {
            // File may not exist yet; ensure the string starts with the base and has no .. (already checked)
            if (strpos($fn, '/opt/fpp/capes/') !== 0) {
                echo "Invalid cape file path\n";
                $file = '';
            } else {
                $file = $fn;
            }
        }

        if ($file !== '' && file_exists('/home/fpp/media/config/cape-eeprom.bin')) {
            unlink('/home/fpp/media/config/co-bbbStrings.json');
            unlink('/home/fpp/media/config/co-pixelStrings.json');
        }

    } else {
        // For uploads, only allow a plain filename (no directory) to stay within uploadDirectory
        $baseName = basename($fn);
        if ($baseName !== $fn || $baseName === '' || strpos($baseName, '/') !== false || strpos($baseName, '\\') !== false) {
            echo "Invalid filename\n";
            $file = '';
        } else {
            $file = $uploadDirectory . '/' . $baseName;
            // Extra containment: ensure parent dir is still uploadDirectory
            $realBase = realpath($uploadDirectory);
            $realParent = realpath(dirname($file));
            if ($realBase && $realParent && strpos($realParent, $realBase) !== 0) {
                echo "Invalid filename\n";
                $file = '';
            }
        }
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
