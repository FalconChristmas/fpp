<?php
    $wrapped = 0;
    $skipJSsettings = 1;
    require_once('common.php');

    
    $force = "";
    if (isset($_GET['force']) && $_GET['force'] == 'true') {
        $force = "force ";
    }
    $file = $_FILES["firmware"]["tmp_name"];
    
    system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $force . $file, $retval);
    WriteSettingToFile('rebootFlag', 1);
    unlink($file);
?>

