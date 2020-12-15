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
Checkout specific Git version
</title>
</head>
<body>
<h2>Checkout specific Git version</h2>
Version: <? echo $_GET['version']; ?><br>
<pre>
<?
} else {
    echo "Checkout specific Git version\n";
    echo "Version: " . $_GET['version'] . "\n";
}

?>
==========================================================================
Switching versions:
<?
system($SUDO . " $fppDir/scripts/git_checkout_version " . $_GET['version']);
?>
==========================================================================
<?
if (!$wrapped) {
?>
</pre>
Switch complete, you may need to reload the page for changes to be visible.<br>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>
</body>
</html>
<?
} else {
    echo "Switch complete, you may need to reload the page for changes to be visible.\n";
}
?>
