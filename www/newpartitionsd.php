<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();


?>

<head>
<title>
New partition
</title>
</head>
<body>
<h2>New partition in unused space</h2>
<pre>
<?php

    $command = "sudo /opt/fpp/SD/newpartition.sh 2>&1";

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
