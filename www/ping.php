<!DOCTYPE html>
<html lang="en">
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

$ip = "github.com";
$count = 5;

if (isset($_GET['ip']))
	$ip = $_GET['ip'];

if (isset($_GET['count']))
	$count = $_GET['count'];

// Each value is one escaped shell word, but one starting with '-' would still
// be read as a ping option, so: an address or host name, and a small count.
if (!is_string($ip) || !(filter_var($ip, FILTER_VALIDATE_IP) ||
		preg_match('/^(?=.{1,253}$)([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?$/', $ip))) {
	$ip = "";
}
$count = max(1, min(100, intval($count)));
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
	if ($ip === "") {
		echo "Invalid host name or address\n";
	} else {
		// The PING header line and error messages echo the requested address
		// verbatim, and this page is injected into the Ping dialog via .html(),
		// so the command output must be escaped. The surrounding <h2>/<pre>
		// markup is static and stays as-is; plain ping output (digits, dots,
		// "time=0.123 ms", stats) contains no HTML-significant characters and
		// renders identically. stderr is merged in so resolution failures are
		// visible instead of rendering an empty box.
		$pingOutput = array();
		exec("ping -c ".escapeshellarg($count)." -n -- ".escapeshellarg($ip)." 2>&1", $pingOutput, $pingRC);
		foreach ($pingOutput as $line) {
			// ENT_SUBSTITUTE keeps a stray non-UTF-8 byte from blanking the
			// whole page (htmlspecialchars returns '' on invalid sequences).
			echo htmlspecialchars($line, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8'), "\n";
		}
	}
	#echo "==================================================================================\n";
	#system("traceroute -n $ip");
?>
==========================================================================
</pre>
</body>
</html>
