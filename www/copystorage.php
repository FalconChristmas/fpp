<?php
$skipJSsettings = 1;
require_once "common.php";
require_once "common/oplog.inc.php";
require_once "common/settings.php";
// Ignore user aborts and allow the script to continue running
session_write_close();
ignore_user_abort(true);
set_time_limit(3600);
header("Access-Control-Allow-Origin: *");

$wrapped = 0;

if (isset($_GET['wrapped']))
    $wrapped = 1;

if (!$wrapped) {
    echo "<!DOCTYPE html>\n";
    echo "<html>\n";
}

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();
if (!$wrapped) {
    ?>

    <head>
        <title>
            Copy Settings
        </title>
    </head>

    <body>
        <h2>Copy Settings</h2>
        <pre>
    <?php
}
$date = date("Ymd-Hi");
// $_GET values bound for the shell below: coerce to string (array input would
// fatal inside escapeshellarg/preg_replace) and quote each shell word
// individually. escapeshellcmd() is NOT a word boundary -- spaces in it split
// into extra argv words for the sudo script.
$getStr = function ($k) {
    return (isset($_GET[$k]) && is_string($_GET[$k])) ? $_GET[$k] : '';
};
// The path is interpolated into the shell command below, so wrap it as a
// single shell argument to prevent command injection.
$path = escapeshellarg(preg_replace('/{DATE}/', $date, $getStr('path')));
// Strict enums mirroring copy_settings_to_storage.sh (which enforces the same
// direction set and coerces anything but "yes" to "no"). Legit callers only
// ever send these values.
$direction = $getStr('direction');
if (!preg_match('/^(TOUSB|FROMUSB|TOLOCAL|FROMLOCAL|TOREMOTE|FROMREMOTE)$/', $direction)) {
    echo "Invalid direction\n";
    exit(1);
}
$compress = ($getStr('compress') === 'yes') ? 'yes' : 'no';
$delete = ($getStr('delete') === 'yes') ? 'yes' : 'no';
$remote_storage = (isset($_GET['remoteStorage']) && is_string($_GET['remoteStorage'])) ? $_GET['remoteStorage'] : 'none';
// flags is legitimately multi-word (space-joined UI checkboxes: "Music
// Videos ..."), consumed word-by-word by the script's case statement -- so
// quote each word instead of the whole string. The case *) branch stays the
// semantic validator for unknown tokens.
$flagWords = preg_split('/\s+/', $getStr('flags'), -1, PREG_SPLIT_NO_EMPTY);
$flagsEscaped = implode(' ', array_map('escapeshellarg', (array)$flagWords));

$tee_log_file = $logDirectory . "/fpp_backup_filecopy.log";
//Remove the log file if it exists before we start
if (file_exists($tee_log_file)) {
    unlink($tee_log_file);
}

$output_header_start = "==================================================================================\n";
echo $output_header_start;
file_put_contents($tee_log_file, $output_header_start);

$command = "sudo stdbuf --output=L " . __DIR__ . "/../scripts/copy_settings_to_storage.sh " . escapeshellarg($getStr('storageLocation')) . " " . $path . " " . escapeshellarg($direction) . " " . escapeshellarg($remote_storage) . " " . escapeshellarg($compress) . " " . escapeshellarg($delete) . " " . $flagsEscaped;

// Breadcrumb the backup into the fppd.log timeline, like every other operation.
//
// fpp_backup_filecopy.log has to keep its name and location for the duration of
// the run: it is not a log but a live progress FEED, polled once a second by
// backup.php (api/file/Logs/...) -- the ONE file under logs/ with a live reader.
// It only needs to exist FOR the run though, so the tail of this script writes
// the content into fppd.log and removes it. backup.php already relies on the
// file disappearing to detect completion (see CopyTimeoutError: "we rely on logs
// api returning a file not found error to signify the copy process ending").
$backupTarget = escapeshellcmd($direction) . ' ' . escapeshellcmd($getStr('storageLocation'));
FppdLogLine('copystorage.php', 'Backup', 'backup START: ' . $backupTarget . ' (detail: logs/fpp_backup_filecopy.log, live)');

$output_command = "Command: " . htmlspecialchars($command) . "\n";
echo $output_command;
file_put_contents($tee_log_file, $output_command, FILE_APPEND);

$output_header_end = "----------------------------------------------------------------------------------\n";
echo $output_header_end;
file_put_contents($tee_log_file, $output_header_end, FILE_APPEND);

// Explicit bash -o pipefail: system() otherwise runs this through /bin/sh
// (dash, no pipefail), so piping through "tee -a" would report tee's exit
// code -- which almost always succeeds regardless of whether the copy
// script itself failed -- rather than copy_settings_to_storage.sh's. With
// pipefail, the pipeline's reported status is the first stage that actually
// failed, so $backupRc (used below for the fppd.log FINISH line) means what
// it always claimed to.
system("/bin/bash -o pipefail -c " . escapeshellarg($command . " 2>&1 | tee -a " . $tee_log_file), $backupRc);
echo "\n";

sleep(2);

// $direction is the validated enum from above (re-reading raw $_GET here
// would fatal inside stripos on array input); $flags recoerced the same way.
$flags = $getStr('flags');
$isRestore = stripos($direction, 'FROM') === 0;
$hasConfig = stripos($flags, 'Configuration') !== false;
$hasPlugins = stripos($flags, 'Plugins') !== false;
$hasEeprom = stripos($flags, 'EEPROM') !== false;

// Restoring "Configuration", "Plugins", or "EEPROM" can bring in state from
// a box that doesn't match what's actually installed/running here --
// Service_* settings, packages, plugin binaries, cape hardware config, and
// more -- the same reconciliation an FPPOS reflash already needs. Rather
// than re-implementing pieces of that reconciliation ourselves (services
// were previously applied inline here; a plugin check was previously done
// inline too), just feed the restore into the exact same boot-time path a
// reflash already uses: handleBootActions() and checkInstallPackages()
// (FPPINIT_Config.cpp) already run every boot and no-op unless triggered, so
// this needs no new C++ -- just the same signal a reflash already produces.
// EEPROM restores don't strictly need BootActions/checkInstallPackages
// (cape hardware config, not packages/services), but setting them anyway
// costs nothing beyond a no-op boot pass, and it matches backup.php's
// virtualEEPROM area -- which gets the same unconditional treatment -- so
// there isn't a separate, narrower code path to maintain for this one case.
// Both markers require a full reboot, not just an fppd restart (fppinit
// start/postNetwork don't run on an fppd-only restart).
//
// Not gated on $backupRc === 0: system()'s pipe here ("... | tee -a
// $tee_log_file") runs via /bin/sh without pipefail, so $backupRc is tee's
// exit code, not copy_settings_to_storage.sh's -- it's ~always 0 regardless
// of whether the actual copy succeeded, so it was never a real success
// check. A partial/failed copy still needs whatever DID land reconciled,
// same reasoning as backup.php's $restore_done-only gate.
if ($isRestore && ($hasConfig || $hasPlugins || $hasEeprom)) {
    WriteSettingToFile('rebootFlag', '1');
    WriteSettingToFile('BootActions', 'settings');
    exec('sudo touch /fppos_upgraded');
}

// Recorded in the plugin install history. An "All" restore replaces that file
// with the source's, so this entry, written after it, marks where the copy
// ends; "Plugins" brings in plugins outside the Plugin Manager, and
// "Configuration" replaces the settings file (plugin source flags included).
$copiedModes = array_values(array_intersect(array('All', 'Configuration', 'Plugins'), (array) $flagWords));
if ($isRestore && !empty($copiedModes)) {
    require_once __DIR__ . '/common/pluginhistory.inc.php';
    AppendPluginHistory('', 'copied', array('modes' => $copiedModes, 'direction' => $direction));
}

// The run's output goes into the fppd.log timeline, and the progress file is
// then removed rather than kept as fpp_backup_filecopy_last.log.
//
// fpp_backup_filecopy.log is a live progress FEED, not a log: it is deleted at
// the start of every run and polled once a second by backup.php while the copy
// runs (api/file/Logs/...), which is why it has to keep its name and location.
// But it only ever needed to exist FOR the run. Renaming it to _last.log left a
// permanent file in logs/ holding exactly one backup -- the previous one was
// overwritten every time -- so it was simultaneously clutter AND a bad archive.
// fppd.log keeps every backup, is rotated, and is already in the Support Zip.
// A run is ~70 lines of rsync summaries (not per-file listings), so this costs
// the timeline very little.
if (is_readable($tee_log_file)) {
    FppdLogLine('copystorage.php', 'Backup', file_get_contents($tee_log_file));
    unlink($tee_log_file);
}
FppdLogLine('copystorage.php', 'Backup', 'backup FINISH: ' . $backupTarget . ' (rc=' . $backupRc . ')');
if (!$wrapped) {
    ?>

    ==========================================================================
    </pre>
        <a href='index.php'>Go to FPP Main Status Page</a><br>
    </body>

    </html>
<? } ?>
