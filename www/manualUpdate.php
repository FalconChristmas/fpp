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
if (file_exists("/.dockerenv")) {
    system($SUDO . " $fppDir/scripts/fppd_stop");
} else {
    exec($SUDO . " systemctl stop fppd");
}

touch("$mediaDirectory/tmp/fppd_restarted");
?>
==========================================================================
Pulling in updates...
<?
system("$fppDir/scripts/git_pull");
?>
==========================================================================
Restarting fppd...
<?
if (file_exists("/.dockerenv")) {
    system($SUDO . " $fppDir/scripts/fppd_start");
} else {
    exec($SUDO . " systemctl restart fppd");
}
exec($SUDO . " rm -f /tmp/cache_*.cache");
?>
==========================================================================
Update Complete.
</pre>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>

</body>
</html>
