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

$script = "";
if ((isset($_GET['scriptName'])) && strlen($_GET['scriptName']) > 0)
{
    // Constrain to a single filename inside the script directory. sanitizeFilename() strips
    // "/" and ".." so the value cannot escape the directory via path traversal, and
    // escapeshellarg() below wraps the resolved path as one shell word.
    $script = sanitizeFilename($_GET['scriptName']);
}

if ($script != "" && file_exists($scriptDirectory . "/" . $script))
{
	$args = "";
	if (isset($_GET['args']))
		$args = escapeshellcmd($_GET['args']);

	if (isset($_GET['nohtml'])) {
		echo "Running $script $args\n--------------------------------------------------------------------------------\n";
		system($SUDO . " $fppDir/scripts/eventScript " . escapeshellarg($scriptDirectory . "/" . $script) . " $args");
	} else {
		echo "Running $script $args<br><hr>\n";
		echo "<pre>\n";
		system($SUDO . " $fppDir/scripts/eventScript " . escapeshellarg($scriptDirectory . "/" . $script) . " $args");
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
