<?php
if (!isset($_GET['nohtml'])) {
?>
<!DOCTYPE html>
<html lang="en">
<?php
}

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!isset($_GET['nohtml'])) {
?>

<head>
<title>
FPP Event Script
</title>
</head>
<body>
<h2>FPP Event Script</h2>

<?php
}

if (isset($_GET['plugin'])) {
    $plugin = sanitizeFilename($_GET['plugin']);
    $scriptDirectory = "/home/fpp/media/plugins/$plugin/scripts";
}

if ((isset($_GET['scriptName'])) && strlen($_GET['scriptName']) > 0 &&
    (file_exists($scriptDirectory . "/" . $_GET['scriptName'])))
{
	$script = escapeshellcmd($_GET['scriptName']);

	$args = "";
	if (isset($_GET['args']))
		$args = escapeshellcmd($_GET['args']);

	if (isset($_GET['nohtml'])) {
		echo "Running $script $args\n--------------------------------------------------------------------------------\n";
		system($SUDO . " $fppDir/scripts/eventScript $scriptDirectory/$script $args");
	} else {
		echo "Running $script $args<br><hr>\n";
		echo "<pre>\n";
		system($SUDO . " $fppDir/scripts/eventScript $scriptDirectory/$script $args");
		echo "</pre>\n";
	}
}
else
{
?>
ERROR: Unknown script:
<?php
	echo htmlspecialchars($_GET['scriptName']);
}

if (!isset($_GET['nohtml'])) {
?>
<br>
</body>
</html>
<?php
}
?>
