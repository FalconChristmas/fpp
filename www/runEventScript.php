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

if (isset($_GET['plugin']) && is_string($_GET['plugin'])) {
    $plugin = sanitizeFilename($_GET['plugin']);
    $scriptDirectory = "/home/fpp/media/plugins/$plugin/scripts";
}

$script = "";
if ((isset($_GET['scriptName'])) && is_string($_GET['scriptName']) && strlen($_GET['scriptName']) > 0)
{
    // Constrain to a single filename inside the script directory. sanitizeFilename() strips
    // "/" and ".." so the value cannot escape the directory via path traversal, and
    // escapeshellarg() below wraps the resolved path as one shell word.
    $script = sanitizeFilename($_GET['scriptName']);
}

if ($script != "" && file_exists($scriptDirectory . "/" . $script))
{
	$argsRaw = (isset($_GET['args']) && is_string($_GET['args'])) ? $_GET['args'] : "";
	// Display context needs HTML escaping (escapeshellcmd does not encode <>"').
	$argsDisplay = htmlspecialchars($argsRaw);
	// Shell context keeps the historical word-splitting -- eventScript passes
	// "$@" through to the target script -- but quotes each word, so shell
	// metacharacters cannot escape. Word count is unchanged vs escapeshellcmd().
	$words = preg_split('/\s+/', $argsRaw, -1, PREG_SPLIT_NO_EMPTY);
	$args = implode(" ", array_map("escapeshellarg", (array)$words));

	if (isset($_GET['nohtml'])) {
		echo "Running $script $argsDisplay\n--------------------------------------------------------------------------------\n";
		system($SUDO . " $fppDir/scripts/eventScript " . escapeshellarg($scriptDirectory . "/" . $script) . " $args");
	} else {
		echo "Running $script $argsDisplay<br><hr>\n";
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
	echo htmlspecialchars(isset($_GET['scriptName']) && is_string($_GET['scriptName']) ? $_GET['scriptName'] : '');
}

if (!isset($_GET['nohtml'])) {
?>
<br>
</body>
</html>
<?php
}
?>
