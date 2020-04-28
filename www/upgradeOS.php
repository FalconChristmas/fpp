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
Image: <? echo $_GET['os']; ?><br>
<pre>
<?
} else {
    echo "FPP OS Upgrade\n";
    echo "Image: " . $_GET['os'] . "\n";
}

if (preg_match('/^https?:/', $_GET['os'])) {
    echo "==========================================================================\n";
    $baseFile = preg_replace('/.*\/([^\/]*)$/', '$1', $_GET['os']);
    echo "Downloading OS Image:\n";
    $cmd = "curl -f --fail-early " . $_GET['os'] . " --output /home/fpp/media/upload/$baseFile 2>&1";
    system($cmd);
    $_GET['os'] = $baseFile;
}
?>
==========================================================================
Upgrading OS:
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
