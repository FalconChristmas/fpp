<!DOCTYPE html>
<html lang="en">
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

$ip = "github.com";
$count = 5;

if (isset($_GET['ip']) && is_string($_GET['ip']))
	$ip = $_GET['ip'];

if (isset($_GET['count']) && is_string($_GET['count']))
	$count = $_GET['count'];
?>

<head>
<title>
Ping
</title>
</head>
<body>
<h2>Ping <? echo htmlspecialchars($ip); ?></h2>
<pre>
==========================================================================
<?php
	// The PING header line and error messages echo the requested address
	// verbatim, and this page is injected into the Ping dialog via .html(),
	// so the command output must be escaped. The surrounding <h2>/<pre>
	// markup is static and stays as-is; plain ping output (digits, dots,
	// "time=0.123 ms", stats) contains no HTML-significant characters and
	// renders identically. stderr is merged in so resolution failures are
	// visible instead of rendering an empty box.
	$pingOutput = array();
	exec("ping -c ".escapeshellarg($count)." -n ".escapeshellarg($ip)." 2>&1", $pingOutput, $pingRC);
	foreach ($pingOutput as $line) {
		// ENT_SUBSTITUTE keeps a stray non-UTF-8 byte from blanking the
		// whole page (htmlspecialchars returns '' on invalid sequences).
		echo htmlspecialchars($line, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8'), "\n";
	}
?>
==========================================================================
</pre>
</body>
</html>
