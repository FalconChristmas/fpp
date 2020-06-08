<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "Missing 'ip' URL argument.\n";
    exit(0);
}

$ip = $_GET['ip'];

$dirs = Array();

if ((isset($settings['MultiSyncCopySequences']) && ($settings['MultiSyncCopySequences'] == 1)) ||
	(!isset($settings['MultiSyncCopySequences'])))
	array_push($dirs, 'sequences');
if (isset($settings['MultiSyncCopyEffects']) && ($settings['MultiSyncCopyEffects'] == 1))
	array_push($dirs, 'effects');
if (isset($settings['MultiSyncCopyVideos']) && ($settings['MultiSyncCopyVideos'] == 1))
	array_push($dirs, 'videos');
if (isset($settings['MultiSyncCopyEvents']) && ($settings['MultiSyncCopyEvents'] == 1))
	array_push($dirs, 'events');
if (isset($settings['MultiSyncCopyScripts']) && ($settings['MultiSyncCopyScripts'] == 1))
	array_push($dirs, 'scripts');
if (isset($settings['MultiSyncCopyMusic']) && ($settings['MultiSyncCopyMusic'] == 1))
    array_push($dirs, 'music');

if (sizeof($dirs) == 0)
{
	echo "You do not have any files set to be copied.  Please select which file types you wish to copy to the remote systems.";
	exit(0);
}

echo "Syncing files to remote FPP system at $ip\n";

foreach ( $dirs as $dir ) {
	echo "==================================================================================\n";
	printf( "Syncing %s dir to %s\n", $dir, $ip );
	$compress = "";
	if (($dir == "sequences") &&
		(isset($settings['CompressMultiSyncTransfers'])) &&
		($settings['CompressMultiSyncTransfers'] == "1"))
	{
		$compress = "-z";
	}

	$command = "rsync -rtDlv --modify-window=1 $compress --stats $fppHome/media/$dir/ $ip::media/$dir/ 2>&1";

	echo "Command: $command\n";
	echo "----------------------------------------------------------------------------------\n";
	system($command);
	echo "\n";
}

?>
--------------
Sync Complete.
