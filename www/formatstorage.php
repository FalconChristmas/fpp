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

		$command = "sudo /opt/fpp/scripts/format_storage.sh " . escapeshellcmd($_GET['fs']) . " " . escapeshellcmd($_GET['storageLocation']) . " 2>&1";

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
