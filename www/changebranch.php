<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();
?>

<head>
<title>
Change Branch
</title>
</head>
<body>
<h2>Changing Branch</h2>
<pre>
<?php
    echo "==================================================================================\n";

    $branch = $_GET['branch'];
	$command = "sudo /opt/fpp/scripts/git_branch " . $branch . " 2>&1";

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
