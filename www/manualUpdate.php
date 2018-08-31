<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

?>

<head>
<title>
FPP Manual Update
</title>
</head>
<body>
<h2>FPP Manual Update</h2>
<pre>
Stopping fppd...
<?php
system($SUDO . " $fppDir/scripts/fppd_stop");
?>
==========================================================================
Pulling in updates...
<?
system("$fppDir/scripts/git_pull");
?>
==========================================================================
Restarting fppd...
<?
system($SUDO . " $fppDir/scripts/fppd_start");
?>
==========================================================================
Update Complete.
</pre>
<a href='/'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>

</body>
</html>
