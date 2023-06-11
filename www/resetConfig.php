<?
header("Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

# Any file not starting with a / is assumed to be under $mediaDirectory
# which is normally /home/fpp/media
$files = array();
$files['scripts'] = array(
    'scripts/*',
);
$files['caches'] = array(
    'cache/*',
    'config/media_durations.cache',
    'tmp/*',
);
$files['backups'] = array(
    'backups/*',
    'config/backups',
);
$files['config'] = array(
    'config/.htaccess',
    'config/.htpasswd',
    'config/authorized_keys',
    'config/dns',
    'config/gpio.json',
    'config/instantCommand.json',
    'config/model-overlays.json',
    'config/virtualdisplaymap',
    'fpp-info.json',
    'exim4/*',
    'config/cape-eeprom.bin',
);
$files['network'] = array(
    'config/interface.*',
    '/var/lib/connman/fpp.config',
);
$files['media'] = array(
    'images/*',
    'music/*',
    'videos/*',
);
$files['sequences'] = array(
    'sequences/*',
);
$files['effects'] = array(
    'effects/*',
);
$files['playlists'] = array(
    'playlists/*',
);
$files['channeloutputs'] = array(
    'config/co-bbbStrings.json',
    'config/channeloutputs.json',
    'config/co-other.json',
    'config/co-pixelStrings.json',
    'config/co-universes.json',
);
$files['schedule'] = array(
    'config/schedule.json',
);
$files['settings'] = array(
    'settings',
);
$files['uploads'] = array(
    'upload/*',
);
$files['logs'] = array(
    'logs/*',
);
$files['plugins'] = array(
    'plugins/*',
);
$files['pluginConfigs'] = array(
    'config/plugin.*',
    'plugindata/*',
);
$files['user'] = array(
    '/home/fpp/.bash_history',
    '/home/fpp/.ssh/authorized_keys',
    '/home/fpp/.ssh/known_hosts',
    '/root/.bash_history',
    '/root/.ssh/authorized_keys',
    '/root/.ssh/known_hosts',
);

?>
Resetting FPP Configuration
======================================================================
Removing files:
<?
$areas = array();
if (isset($_GET['areas'])) {
    $areasStr = $_GET['areas'];
    $areasStr = preg_replace('/,dummy/', '', $areasStr);
    $areas = preg_split('/,/', $areasStr);
}

foreach ($areas as $area) {
    printf("Area: %s\n", $area);
    foreach ($files[$area] as $file) {
        $base = $settings['mediaDirectory'] . '/';
        if (substr($file, 0, 1) == '/') {
            $base = '';
        }
        if (preg_match('/[?\*]/', $file)) {
            $filesInDir = glob($base . $file); // get all file names
            foreach ($filesInDir as $fileToRemove) { // iterate files
                if (is_file($fileToRemove)) {
                    printf("%s\n", $fileToRemove);
                    unlink($fileToRemove); // delete file
                } else if (is_dir($fileToRemove)) {
                    $di = new RecursiveDirectoryIterator($fileToRemove, FilesystemIterator::SKIP_DOTS);
                    $ri = new RecursiveIteratorIterator($di, RecursiveIteratorIterator::CHILD_FIRST);
                    foreach ($ri as $fileToRemove2) {
                        printf("%s\n", $fileToRemove2);
                        $fileToRemove2->isDir() ? rmdir($fileToRemove2) : unlink($fileToRemove2);
                    }
                    rmdir($fileToRemove);
                }
            }
        } else if (file_exists($base . $file)) {
            printf("%s%s\n", $base, $file);
            unlink($base . $file); // delete file
        }
    }
    printf("\n");
}

flush();
?>
