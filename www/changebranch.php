<?php
header("Access-Control-Allow-Origin: *");
?>
<!DOCTYPE html>
<html lang="en">
<?
include 'common/htmlMeta.inc';
$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();
?>

<head>
	<title>
		Change Branch
	</title>
</head>

<body>
	<h2>Changing Branch</h2>
	<pre id="output">
<?php
echo "==================================================================================\n";

$branch = escapeshellcmd(htmlspecialchars($_GET['branch']));
$remote = isset($_GET['remote']) ? escapeshellcmd(htmlspecialchars($_GET['remote'])) : 'origin';

// Validate remote name to prevent injection
if (!preg_match('/^[a-zA-Z0-9_-]+$/', $remote)) {
	$remote = 'origin';
}

$command = "$SUDO " . $fppDir . "/scripts/git_branch " . $branch . " " . $remote . " 2>&1";

echo "Command: $command\n";
echo "----------------------------------------------------------------------------------\n";
flush();
ob_flush();

// Stream output and auto-scroll
$handle = popen($command, 'r');
while (!feof($handle)) {
	$buffer = fgets($handle);
	echo $buffer;
	flush();
	ob_flush();
	echo '<script>window.scrollTo(0, document.body.scrollHeight);</script>';
	flush();
	ob_flush();
}
pclose($handle);
echo "\n";
?>

==========================================================================
</pre>
	<a href='index.php'>Go to FPP Main Status Page</a><br>
	<script>
		// Ensure we scroll to bottom on page load
		window.scrollTo(0, document.body.scrollHeight);
	</script>
</body>

</html>