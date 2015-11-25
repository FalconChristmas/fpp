<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

?>

<html>
<head>
<title>
FPP Event Script
</title>
</head>
<body>
<h2>FPP Event Script</h2>

<?php
if ((isset($_GET['scriptName'])) &&
    (file_exists($scriptDirectory . "/" . $_GET['scriptName'])))
{
	$script = $_GET['scriptName'];
	echo "Running $script<br><hr>\n";
	echo "<pre>\n";
	system($SUDO . " $fppDir/scripts/eventScript $scriptDirectory/$script");
	echo "</pre>\n";
}
else
{
?>
ERROR: Unknown script:
<?
	echo $_GET['scriptName'];
}
?>
<br>
</body>
</html>
