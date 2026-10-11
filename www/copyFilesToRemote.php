<?php

$skipJSsettings = 1;
require_once("common.php");
require_once("common/remoteJobs.inc.php");

DisableOutputBuffering();

if (!isset($_GET['ip'])) {
    echo "Missing 'ip' URL argument.\n";
    exit(0);
}

$rawIp = $_GET['ip'];
// Validate IP or hostname strictly — reject shell metachars/spaces.
// filter_var handles IPv4/IPv6; hostname regex covers simple DNS names (RFC 1123) used for local FPP hosts.
$validIp = filter_var($rawIp, FILTER_VALIDATE_IP);
$validHost = preg_match('/^(?=.{1,253}$)([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?$/', $rawIp);
if (!$validIp && !$validHost) {
    echo "Invalid 'ip' URL argument.\n";
    exit(0);
}
$ip = $rawIp;

$dirs = MultiSyncCopyDirs();

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

	// Quote src and dest as single shell args so the shell cannot split on space/;.
	// $fppHome is trusted (from mediaDirectory) but quote for consistency; $ip is validated above.
	$src = escapeshellarg($fppHome . "/media/" . $dir . "/");
	$dest = escapeshellarg($ip . "::media/" . $dir . "/");
	$command = "rsync -rtDlv --modify-window=1 $compress --stats $src $dest 2>&1";

	echo "Command: $command\n";
	echo "----------------------------------------------------------------------------------\n";
	system($command);
	echo "\n";
}

?>
--------------
Sync Complete.
