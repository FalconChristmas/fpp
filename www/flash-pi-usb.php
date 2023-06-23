<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();


?>

<head>
<title>
Flash to USB
</title>
</head>
<body>
<h2>Flash to USB</h2>
<pre>
<?php
        echo "==================================================================================\n";
        
        $command = "sudo TERM=vt100 env 2>&1";
        
        echo "Command: $command\n";
        echo "----------------------------------------------------------------------------------\n";
        system($command);
    
		echo "==================================================================================\n";

        $command = "sudo TERM=vt100 /opt/fpp/SD/Pi-FlashUSB.sh ext4 2>&1";

		echo "Command: $command\n";
		echo "----------------------------------------------------------------------------------\n";
		system($command);
		echo "\n";

?>

==========================================================================
</pre>
<?php
if (file_exists("/boot/recovery.bin")) {
    WriteSettingToFile("rebootFlag", "1");
}

?>
<a href='index.php'>Go to FPP Main Status Page</a><br>
</body>
</html>
