<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/backups/list
function GetAvailableBackupsFromDir($backupDir) {
    $excludeList = Array();
    $dirs = Array();

    foreach(scandir('/home/fpp/media') as $fileName) {
        if (($fileName != '.') &&
            ($fileName != '..')) {
            array_push($excludeList, $fileName);
        }
    }

    array_push($dirs, '/');

    foreach (scandir($backupDir) as $fileName) {
        if (($fileName != '.') &&
            ($fileName != '..') &&
            (!in_array($fileName, $excludeList)) &&
            (is_dir($backupDir . '/' . $fileName))) {
            array_push($dirs, $fileName);

            foreach (scandir($backupDir . '/' . $fileName) as $subfileName) {
                if (($subfileName != '.') &&
                    ($subfileName != '..') &&
                    (!in_array($subfileName, $excludeList))) {
                    array_push($dirs, $fileName . '/' . $subfileName);
                }
            }
        }
    }

    return $dirs;
}

function GetAvailableBackups() {
    return json(GetAvailableBackupsFromDir('/home/fpp/media/backups/'));
}

function CheckIfDeviceIsUsable($deviceName) {
    global $SUDO;

    // Check if in use / Mount / List / Unmount
    $mountPoint = exec($SUDO . " lsblk /dev/$deviceName");
    $mountPoint = preg_replace('/.*disk ?/', '', $mountPoint);
    $mountPoint = preg_replace('/.*part ?/', '', $mountPoint);
    if (preg_match('/[a-z0-9\/]/', $mountPoint))
        return "ERROR: Partition is mounted on: $mountPoint";

    $isSwap = exec("grep /dev/$deviceName /proc/swaps");
    if ($isSwap != "")
        return "ERROR: $deviceName is a swap partition";

    return "";
}

function GetAvailableBackupsDevices() {
    global $SUDO;
    $devices = Array();

    foreach(scandir("/dev/") as $deviceName) {
        if (preg_match("/^sd[a-z][0-9]/", $deviceName)) {
            exec($SUDO . " sfdisk -s /dev/$deviceName", $output, $return_val);
            $GB = round(intval($output[0]) / 1024.0 / 1024.0, 1);
            unset($output);

            if ($GB <= 0.1)
                continue;

            $unusable = CheckIfDeviceIsUsable($deviceName);
            if ($unusable != '')
                continue;

            $baseDevice = preg_replace('/[0-9]*$/', '', $deviceName);

            $device = Array();
            $device['name'] = $deviceName;
            $device['size'] = $GB;
            $device['model'] = exec("cat /sys/block/$baseDevice/device/model");
            $device['vendor'] = exec("cat /sys/block/$baseDevice/device/vendor");

            array_push($devices, $device);
        }
    }

    return json($devices);
}

function GetAvailableBackupsOnDevice() {
    global $SUDO;
    $deviceName = params('DeviceName');
    $dirs = Array();

    // unmount just in case
    exec($SUDO . ' umount /mnt/tmp');

    $unusable = CheckIfDeviceIsUsable($deviceName);
    if ($unusable != '') {
        array_push($dirs, $unusable);
        return json($dirs);
    }

    exec($SUDO . ' mkdir -p /mnt/tmp');

    $fsType = exec($SUDO . ' file -sL /dev/' . $deviceName, $output);

    $mountCmd = '';
    // Same mount options used in scripts/copy_settings_to_storage.sh
    if (preg_match('/BTRFS/', $fsType)) {
        $mountCmd = "mount -t btrfs -o noatime,nodiratime,compress=zstd,nofail /dev/$deviceName /mnt/tmp";
    } else if ((preg_match('/FAT/', $fsType)) ||
               (preg_match('/DOS/', $fsType))) {
        $mountCmd = "mount -t auto -o noatime,nodiratime,exec,nofail,flush,uid=500,gid=500 /dev/$deviceName /mnt/tmp";
    } else {
        // Default to ext4
        $mountCmd = "mount -t ext4 -o noatime,nodiratime,nofail /dev/$deviceName /mnt/tmp";
    }

    exec($SUDO . ' ' . $mountCmd);

    $dirs = GetAvailableBackupsFromDir('/mnt/tmp/');

    exec($SUDO . ' umount /mnt/tmp');

    return json($dirs);
}

?>
