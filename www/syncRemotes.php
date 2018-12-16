<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

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

if (sizeof($dirs) == 0)
{
	echo "You do not have any files set to be copied.  Please return to the <a href='/multisync.php'>MultiSync setup page</a> and select which files you wish to copy to the remotes.";
	exit(0);
}

$remotes = Array();
if (!isset($settings['fppMode']) || ($settings['fppMode'] != 'master')) {
	echo "ERROR: Only Master FPP instances can sync to Remotes.\n";
	exit(0);
} else if ( isset($_GET['MultiSyncRemotes']) && !empty($_GET['MultiSyncRemotes'])) {
	$remotes = preg_split('/,/', $_GET['MultiSyncRemotes']);
} else if ( isset($settings['MultiSyncRemotes']) && !empty($settings['MultiSyncRemotes'])) {
	if ( $settings['MultiSyncRemotes'] != "255.255.255.255" ) {
		$remotes = preg_split('/,/', $settings['MultiSyncRemotes']);
	} else {
		exec("ip addr show up | grep 'inet ' | awk '{print $2}' | cut -f1 -d/ | grep -v '^127'", $localIPs);

		exec("avahi-browse -artp | grep -v 'IPv6' | sort", $rmtSysOut);

		$uniqRemotes = Array();
		$remotes = Array();

		foreach ($rmtSysOut as $system)
		{
			if (!preg_match("/^=.*fpp-fppd/", $system))
				continue;
			if (!preg_match("/fppMode/", $system))
				continue;

			$parts = explode(';', $system);

			$matches = preg_grep("/" . $parts[7] . "/", $localIPs);
			if ((!count($matches)) && (count($parts) > 8))
			{
				$mode = "unknown";

				$txtParts = explode(',', preg_replace("/\"/", "", $parts[9]));

				foreach ($txtParts as $txtPart)
				{
					$kvPair = explode('=', $txtPart);
					if ($kvPair[0] == "fppMode")
						$mode = $kvPair[1];
				}

				if ($mode == "remote")
					$uniqRemotes[$parts[7]] = 1;
			}
		}

		foreach ($uniqRemotes as $ip => $v)
		{
			array_push($remotes, $ip);
		}
	}
} else {
	echo "No remotes configured and no list supplied.\n";
	exit(0);
}

?>

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

		$command = "rsync -rtDlv --modify-window=1 $compress --stats $fppHome/media/$dir/ $remote::media/$dir/ 2>&1";

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
