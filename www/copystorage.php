<?php
$skipJSsettings = 1;
include "./common.php";
// Ignore user aborts and allow the script to continue running
session_write_close();
ignore_user_abort(true);
set_time_limit(3600);
header( "Access-Control-Allow-Origin: *");

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

    $command = "sudo stdbuf --output=L " . __DIR__ . "/../scripts/copy_settings_to_storage.sh " . escapeshellcmd($_GET['storageLocation']) . " " . $path . " " . escapeshellcmd($_GET['direction']) . " " . $remote_storage . " " .  $compress . " " . $delete . " " . escapeshellcmd($_GET['flags']);

        $output_command = "Command: ".htmlspecialchars($command)."\n";
		echo $output_command;
        file_put_contents($tee_log_file, $output_command, FILE_APPEND);

        $output_header_end = "----------------------------------------------------------------------------------\n";
        echo $output_header_end;
        file_put_contents($tee_log_file, $output_header_end, FILE_APPEND);

        system($command . " 2>&1 | tee -a " . $tee_log_file);
		echo "\n";

        sleep (2);

        #finally rename the log file since we're done
        $new_filename = str_replace(".log", "_last.log", $tee_log_file);
        rename($tee_log_file, $new_filename);
if (!$wrapped) {
?>

==========================================================================
</pre>
<a href='index.php'>Go to FPP Main Status Page</a><br>
</body>
</html>
<? } ?>
