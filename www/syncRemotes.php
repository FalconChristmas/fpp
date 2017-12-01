<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

$dirs = Array(
	'sequences',
	'videos'
	);

$remotes = Array();
if ( isset($_GET['MultiSyncRemotes']) && !empty($_GET['MultiSyncRemotes'])) {
	$remotes = preg_split('/,/', $_GET['MultiSyncRemotes']);
} else if ( isset($settings['MultiSyncRemotes']) && !empty($settings['MultiSyncRemotes'])) {
	if ( $settings['MultiSyncRemotes'] != "255.255.255.255" ) {
		$remotes = preg_split('/,/', $settings['MultiSyncRemotes']);
	} else {
		echo "Sync Remotes does not currently work with the 'ALL Remotes' option.\n";
		exit(0);
	}
} else {
	echo "No remotes configured and no list supplied.\n";
	exit(0);
}

?>

<html>
<head>
<title>
Sync Remotes
</title>
</head>
<body>
<h2>FPP MultiSync Remote File Sync</h2>
<pre>
<?php
foreach ( $remotes as $remote ) {
	foreach ( $dirs as $dir ) {
		echo "==================================================================================\n";
		printf( "Syncing %s dir to %s\n", $dir, $remote );
		$compress = "";
		if (($dir == "sequences") &&
			(isset($settings['CompressMultiSyncTransfers'])) &&
			($settings['CompressMultiSyncTransfers'] == "1"))
		{
			$compress = "-z";
		}

		$command = "rsync -av --modify-window=1 $compress --stats $fppHome/media/$dir/ $remote::media/$dir/ 2>&1";

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";
	}
}

?>

==========================================================================
Sync Complete.
</pre>
<a href='/'>Go to FPP Main Status Page</a><br>
<a href='multisync.php'>Go to MultiSync config page</a><br>

</body>
</html>
