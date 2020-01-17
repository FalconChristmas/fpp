<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();


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
    $date = date("Ymd-Hi");
    $path = preg_replace('/{DATE}/', $date, $_GET['path']);

		echo "==================================================================================\n";

    $command = "sudo /opt/fpp/scripts/copy_settings_to_storage.sh " . $_GET['storageLocation'] . " " . $path . " " . $_GET['direction']  . " " . $_GET['delete'] . " " . $_GET['flags'] . " 2>&1";

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
