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
	<pre>
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
system($command);
echo "\n";
?>

==========================================================================
</pre>
	<a href='index.php'>Go to FPP Main Status Page</a><br>
</body>

</html>