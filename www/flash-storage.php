<?
header("Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

// The target must be one of the disks we actually offered.  The old endpoint passed
// whatever ?dev= contained straight through to a root shell script (escaped, but
// unvalidated), so a request could name the booted disk or the media drive.
$requested = isset($_GET['dev']) ? $_GET['dev'] : '';
$targets = GetFlashTargetDevices();

$device = '';
foreach ($targets as $t) {
    if ($t['name'] === $requested) {
        $device = $t['name'];
        break;
    }
}

if ($device === '') {
    echo "ERROR: '" . htmlspecialchars($requested) . "' is not a valid target device.\n";
    echo "It is either the booted disk, the storage device, or not present.\n";
    return;
}

// "create" builds a clean install that can run beside this one; "copy" moves this
// install, media and identity included, onto another disk.
$mode = (isset($_GET['mode']) && $_GET['mode'] === 'copy') ? '--clone' : '--fresh';

$extra = '';
if (isset($_GET['btrfs']) && $_GET['btrfs'] === 'true') {
    $extra = ' --btrfs';
}

// The eMMC flow has always powered the machine down when finished, so that the user
// pulls the SD card and boots from the copy.
if (isset($_GET['reboot']) && $_GET['reboot'] === 'true') {
    $extra .= ' --reboot';
}

$command = "sudo TERM=vt100 /opt/fpp/SD/flash_storage.sh -y " . $mode . $extra . " " . escapeshellarg($device) . " 2>&1";

echo "==================================================================================\n";
echo "Command: $command\n";
echo "----------------------------------------------------------------------------------\n";

system($command, $exitCode);

echo "\n----------------------------------------------------------------------------------\n";
if ($exitCode !== 0) {
    echo "FAILED (exit $exitCode) -- /dev/$device is not usable.  See the errors above.\n";
}

if (file_exists(GetDirSetting('boot') . "/recovery.bin")) {
    WriteSettingToFile("rebootFlag", "1");
}
?>
