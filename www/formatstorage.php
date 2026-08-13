<!DOCTYPE html>
<html lang="en">
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();


?>

<head>
<title>
Reformat Storage Filesystem
</title>
</head>
<body>
<h2>Reformat Storage Filesystem</h2>
<pre>
<?php
		echo "==================================================================================\n";

		$fs = $_GET['fs'];
		$storageLocation = $_GET['storageLocation'];

		$validFS = array('FAT', 'ext4', 'exFAT', 'btrfs');
		if (!in_array($fs, $validFS)) {
			echo "ERROR: Invalid filesystem type requested.\n";
			echo "==========================================================================\n";
			exit;
		}

		if (!preg_match('/^(sd[a-z][0-9]+|mmcblk[0-9]+p[0-9]+|nvme[0-9]+n[0-9]+p[0-9]+)$/', $storageLocation)) {
			echo "ERROR: Invalid storage device requested.\n";
			echo "==========================================================================\n";
			exit;
		}

		// Refuse to format anything currently mounted - this is the only server-side
		// safety net preventing a format request from wiping the running SD/NVMe
		// install, the active media storage, or a backup drive that's still in use.
		// This also naturally covers root/boot, since those are always mounted.
		$mountedError = CheckIfDeviceIsUsable($storageLocation);
		if ($mountedError != "") {
			echo "ERROR: Refusing to format '$storageLocation' - $mountedError\n";
			echo "==========================================================================\n";
			exit;
		}

		$command = "sudo /opt/fpp/scripts/format_storage.sh " . escapeshellarg($fs) . " " . escapeshellarg($storageLocation) . " 2>&1";

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
        system($command);
		echo "\n";

?>

==========================================================================
</pre>
<a href='/'>Go to FPP Main Status Page</a><br>
</body>
</html>
