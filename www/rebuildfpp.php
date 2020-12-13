<?
header( "Access-Control-Allow-Origin: *");

$wrapped = 0;

if (isset($_GET['wrapped']))
    $wrapped = 1;

if (!$wrapped)
    echo "<html>\n";

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!$wrapped) {
?>
<head>
<title>
Rebuild FPP
</title>
</head>
<body>
<h2>Rebuild FPP</h2>
<pre>
<?
}
?>
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
Rebuilding FPP...
<?
system($SUDO . " $fppDir/scripts/fpp_build");
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
Rebuild Complete.
<?
if (!$wrapped) {
?>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>

</body>
</html>
<?
}
?>

