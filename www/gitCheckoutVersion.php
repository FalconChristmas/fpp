<?
header("Access-Control-Allow-Origin: *");

$wrapped = 0;

if (isset($_GET['wrapped'])) {
    $wrapped = 1;
}

if (!$wrapped) {
    echo "<html>\n";
}

$skipJSsettings = 1;
require_once "common.php";
// Atomic server-side update lock (see common/updateLock.inc.php).
require_once "common/updateLock.inc.php";

DisableOutputBuffering();

// Hold the update lock for the whole checkout+rebuild so a concurrent
// starter gets 409 instead of stacking a second mutation on top. Also
// survive a closed tab mid-run like the other update endpoints.
ignore_user_abort(true);

$rawVersion = $_GET['version'] ?? '';
// Allow branch/tag/SHA names (e.g. master, v10.0, HEAD, a1b2c3d4) — letters, numbers, _, ., -, / only, no .. or leading -.
if (!preg_match('/^[A-Za-z0-9_.\/-]+$/', $rawVersion) || strpos($rawVersion, '..') !== false || $rawVersion === '' || $rawVersion[0] === '-') {
    http_response_code(400);
    echo "Invalid version";
    exit(0);
}
$version = $rawVersion;
UpdateLockAcquireOrConflict('fpp-version-checkout', $version);
if (!$wrapped) {
    ?>
<head>
<title>
Checkout specific Git version
</title>
</head>
<body>
<h2>Checkout specific Git version</h2>
Version: <?echo ($version); ?><br>
<pre>
<?
} else {
    echo "Checkout specific Git version\n";
    echo "Version: $version\n";
}

?>
==========================================================================
Switching versions:
<?
system($SUDO . " " . escapeshellarg($fppDir . "/scripts/git_checkout_version") . " " . escapeshellarg($version));
?>
==========================================================================
<?
if (!$wrapped) {
    ?>
</pre>
Switch complete, you may need to reload the page for changes to be visible.<br>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP System Upgrade page</a><br>
</body>
</html>
<?
} else {
    echo "Switch complete, you may need to reload the page for changes to be visible.\n";
}
?>
