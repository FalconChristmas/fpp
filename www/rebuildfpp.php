<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();
?>

<head>
<title>
Rebuild FPP
</title>
</head>
<body>
<h2>Rebuilding FPP</h2>
<pre>
<?php
    echo "==================================================================================\n";

	$command = "sudo /opt/fpp/scripts/fpp_rebuild 2>&1";

	echo "Command: $command\n";
	echo "----------------------------------------------------------------------------------\n";
    system($command);
	echo "\n";
?>

==========================================================================
</pre>
<a href='index.php'>Go to FPP Main Status Page</a><br>
</body>
</html>
