<?php

$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();


?>

<html>
<head>
<title>
Grow SD Card Filesystem
</title>
</head>
<body>
<h2>Grow SD Card Filesystem</h2>
<pre>
<?php

if ($settings['Platform'] == "BeagleBone Black") {
    $command = "sudo /opt/scripts/tools/grow_partition.sh 2>&1";
} else if ($settings['Platform'] == "Raspberry Pi") {
    $command = "sudo /usr/bin/raspi-config --expand-rootfs";
}

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
