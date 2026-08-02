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
$path = preg_replace('/{DATE}/', $date, $_GET['path']);
$compress = isset($_GET['compress']) ? escapeshellcmd($_GET['compress']) : "no";
$delete = isset($_GET['delete']) ? escapeshellcmd($_GET['delete']) : "no";
$remote_storage = isset($_GET['remoteStorage']) ? escapeshellcmd($_GET['remoteStorage']) : 'none';

$tee_log_file = $logDirectory . "/fpp_backup_filecopy.log";
//Remove the log file if it exists before we start
if (file_exists($tee_log_file)) {
    unlink($tee_log_file);
}

$output_header_start = "==================================================================================\n";
echo $output_header_start;
file_put_contents($tee_log_file, $output_header_start);

$command = "sudo stdbuf --output=L " . __DIR__ . "/../scripts/copy_settings_to_storage.sh " . escapeshellcmd($_GET['storageLocation']) . " " . $path . " " . escapeshellcmd($_GET['direction']) . " " . $remote_storage . " " . $compress . " " . $delete . " " . escapeshellcmd($_GET['flags']);

// Breadcrumb the backup into the fppd.log timeline, like every other operation.
//
// fpp_backup_filecopy.log has to keep its name and location for the duration of
// the run: it is not a log but a live progress FEED, polled once a second by
// backup.php (api/file/Logs/...) -- the ONE file under logs/ with a live reader.
// It only needs to exist FOR the run though, so the tail of this script writes
// the content into fppd.log and removes it. backup.php already relies on the
// file disappearing to detect completion (see CopyTimeoutError: "we rely on logs
// api returning a file not found error to signify the copy process ending").
$backupTarget = escapeshellcmd($_GET['direction']) . ' ' . escapeshellcmd($_GET['storageLocation']);
FppdLogLine('copystorage.php', 'Backup', 'backup START: ' . $backupTarget . ' (detail: logs/fpp_backup_filecopy.log, live)');

$output_command = "Command: " . htmlspecialchars($command) . "\n";
echo $output_command;
file_put_contents($tee_log_file, $output_command, FILE_APPEND);

$output_header_end = "----------------------------------------------------------------------------------\n";
echo $output_header_end;
file_put_contents($tee_log_file, $output_header_end, FILE_APPEND);

system($command . " 2>&1 | tee -a " . $tee_log_file, $backupRc);
echo "\n";

sleep(2);

$direction = $_GET['direction'] ?? '';
$flags = $_GET['flags'] ?? '';
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
