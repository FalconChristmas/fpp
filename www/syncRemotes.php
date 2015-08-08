<?php

$skipJSsettings = 1;
require_once("common.php");

// tell php to automatically flush after every output
// including lines of output produced by shell commands
disable_ob();

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

ob_implicit_flush(true);
flush();

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
		$command = "rsync -av --stats /home/fpp/media/$dir/ $remote::media/$dir/";
		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";
	}
}

echo "==========================================================================\n";
echo "Sync Complete.\n";
echo "</pre>\n";
echo "<a href='/'>Go to FPP Main Status Page</a><br>\n";
echo "<a href='multisync.php'>Go to MultiSync config page</a><br>\n";

/////////////////////////////////////////////////////////////////////////////

function echo_shell($command) {
	// from http://stackoverflow.com/questions/1281140/run-process-with-realtime-output-in-php

	$descriptorspec = array(
		0 => array("pipe", "r"),
		1 => array("pipe", "w"),
		2 => array("pipe", "w")
	);

	flush();
	$process = proc_open($command, $descriptorspec, $pipes, realpath('./'), array());

	if (is_resource($process)) {
		while ($s = fgets($pipes[1])) {
			print $s;
			flush();
		}
	}
}

function disable_ob() {
	// Turn off output buffering
	ini_set('output_buffering', 'off');

	// Turn off PHP output compression
	ini_set('zlib.output_compression', false);

	// Implicitly flush the buffer(s)
	ini_set('implicit_flush', true);
	ob_implicit_flush(true);

	// Clear, and turn off output buffering
	while (ob_get_level() > 0) {
		// Get the curent level
		$level = ob_get_level();
		// End the buffering
		ob_end_clean();
		// If the current level has not changed, abort
		if (ob_get_level() == $level) break;
	}

	// Disable apache output buffering/compression
	if (function_exists('apache_setenv')) {
		apache_setenv('no-gzip', '1');
		apache_setenv('dont-vary', '1');
	}
}

?>
</body>
</html>
