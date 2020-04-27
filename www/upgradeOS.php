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
FPP OS Uprade
</title>
</head>
<body>
<h2>FPP OS Upgrade</h2>
<pre>
<?
} else {
    echo "FPP OS Upgrade\n";
}
?>
==========================================================================
<?
system($SUDO . " $fppDir/SD/upgradeOS-part1.sh /home/fpp/media/upload/" . $_GET['os']);
?>
==========================================================================
<?
if (!$wrapped) {
?>
</pre>
<b>Rebooting.... please wait for FPP to reboot.</b>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>
</body>
</html>
<?
} else {
    echo "Rebooting.... Please wait for FPP to reboot.\n";
}
?>
