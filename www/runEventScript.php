<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

?>

<head>
<title>
FPP Event Script
</title>
</head>
<body>
<h2>FPP Event Script</h2>

<?php

if ((isset($_GET['scriptName'])) && strlen($_GET['scriptName']) > 0 &&
    (file_exists($scriptDirectory . "/" . $_GET['scriptName'])))
{
	$script = escapeshellcmd($_GET['scriptName']);

	$args = "";
	if (isset($_GET['args']))
		$args = escapeshellcmd($_GET['args']);

	echo "Running $script $args<br><hr>\n";
	echo "<pre>\n";
	system($SUDO . " $fppDir/scripts/eventScript $scriptDirectory/$script $args");
	echo "</pre>\n";
}
else
{
?>
ERROR: Unknown script:
<?
	echo htmlspecialchars($_GET['scriptName']);
}
?>
<br>
</body>
</html>
