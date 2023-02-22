<?
header("Access-Control-Allow-Origin: *");

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

$rm = "rm -rfv";
#$rm = "echo"; # uncomment for debugging

# Be careful of things in here since 'rm -rf' is used to wipe subdirs.
# Any file not starting with a / is assumed to be under $mediaDirectory
# which is normally /home/fpp/media
$files = array();
$files['all'] = array(
    'backups/*',
    'cache/*',
    'config/backups',
    'config/cape-eeprom.bin',
    'config/media_durations.cache',
    'scripts/*',
    'tmp/*'
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
    'exim4/*'
);
$files['network'] = array(
    'config/interface.*'
);
$files['media'] = array(
    'images/*',
    'music/*',
    'videos/*'
);
$files['sequence'] = array(
    'effects/*',
    'sequences/*'
);
$files['playlists'] = array(
    'playlists/*'
);
$files['channeloutputs'] = array(
    'config/co-bbbStrings.json',
    'config/channeloutputs.json',
    'config/co-other.json',
    'config/co-pixelStrings.json'
);
$files['schedule'] = array(
    'config/schedule.json'
);
$files['settings'] = array(
    'settings'
);
$files['uploads'] = array(
    'upload/*'
);
$files['logs'] = array(
    'logs/*'
);
$files['plugins'] = array(
    'plugins/*'
);
$files['pluginConfigs'] = array(
    'config/plugin.*',
    'plugindata/*'
);
$files['user'] = array(
    '/home/fpp/.bash_history',
    '/home/fpp/.ssh/authorized_keys',
    '/home/fpp/.ssh/known_hosts',
    '/root/.bash_history',
    '/root/.ssh/authorized_keys',
    '/root/.ssh/known_hosts'
);

# fill out our 'all' category with everything else except Networking
foreach ($files as $k => $v) {
    if ($k != 'all') {
        foreach ($v as $f) {
            if ($f != 'network')
                array_push($files['all'], $f);
        }
    }
}

?>
Resetting FPP Configuration
======================================================================
Removing files:
<?
$areas = array('all');
if (isset($_GET['areas'])) {
    $areasStr = $_GET['areas'];
    $areasStr = preg_replace('/,dummy/', '', $areasStr);
    $areas = preg_split('/,/', $areasStr);

    if (in_array('all', $areas))
        $areas = array('all');
}

foreach ($areas as $area) {
    printf( "Area: %s\n", $area);
    foreach ($files[$area] as $file) {
        $base = $settings['mediaDirectory'] . '/';
        if (substr($file, 0, 1) == '/') {
            $base = '';
        }

        if (preg_match('/[?\*]/', $file)) {
            printf( "%s%s\n", $base, $file);
            system("/bin/ls -1 $base$file | sudo xargs $rm");
        } else {
            if (file_exists($base . $file)) {
                printf( "%s%s\n", $base, $file);
                system("sudo $rm $base$file");
            }
        }
    }
    printf( "\n");
}

flush();
?>
