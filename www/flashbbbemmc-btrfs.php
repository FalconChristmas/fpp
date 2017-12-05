<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();


?>

<html>
<head>
<title>
Flash BBB eMMC
</title>
</head>
<body>
<h2>Flash BBB eMMC</h2>
<pre>
<?php
        echo "==================================================================================\n";
        
        $command = "sudo TERM=vt100 env 2>&1";
        
        echo "Command: $command\n";
        echo "----------------------------------------------------------------------------------\n";
        system($command);
    
		echo "==================================================================================\n";

		$command = "sudo TERM=vt100 /opt/fpp/SD/BBB-FlashMMC.sh 2>&1";

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
