<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

?>

<head>
<title>
FPP OS Uprade
</title>
</head>
<body>
<h2>FPP OS Upgrade</h2>
<pre>
==========================================================================
<?
system($SUDO . " $fppDir/SD/upgradeOS-part1.sh /home/fpp/media/upload/" . $_GET['os']);
?>
==========================================================================
</pre>
<b>Rebooting.... please wait for FPP to reboot.</b>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>
</body>
</html>
