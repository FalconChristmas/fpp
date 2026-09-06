<?
header( "Access-Control-Allow-Origin: *");

$wrapped = 1;
$rawVersion = $_GET['version'] ?? '';
// Allow HEAD, version tags, branch names, or SHA — same as gitCheckoutVersion, plus slash for feature branches.
if ($rawVersion === '' || !preg_match('/^[A-Za-z0-9_.\/-]+$/', $rawVersion) || strpos($rawVersion, '..') !== false || $rawVersion[0] === '-') {
    http_response_code(400);
    echo "Invalid version";
    exit(0);
}
$version = $rawVersion;

if (isset($_GET['wrapped']))
    $wrapped = 1;

if (!$wrapped)
    echo "<html>\n";

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!$wrapped) {
?>
<head>
<title>
Upgrading FPP
</title>
<script type="text/javascript" src="js/jquery-latest.min.js?ref=<?= filemtime('js/jquery-latest.min.js'); ?>"></script>
<script type="text/javascript" src="js/jquery-ui.min.js?ref=<?= filemtime('js/jquery-ui.min.js'); ?>"></script>
<script type="text/javascript" src="js/jquery.ui.touch-punch.js?ref=<?= filemtime('js/jquery.ui.touch-punch.js'); ?>"></script>
<script type="text/javascript" src="js/jquery.jgrowl.min.js?ref=<?= filemtime('js/jquery.jgrowl.min.js'); ?>"></script>
<link rel="stylesheet" href="css/jquery.jgrowl.min.css?ref=<?= filemtime('css/jquery.jgrowl.min.css'); ?>" />
<script>
function Reboot() {
    $.get({
        url: "api/system/reboot",
        data: "",
        success: function(data) {
            //Show FPP is rebooting notification for 60 seconds then reload the page
            $.jGrowl('FPP is rebooting..', {life: 60000},{themeState:'detract'});
            setTimeout(function () {
                    location.href="index.php";
            }, 60000);
        }
    });    
}
</script>
</head>
<body>
<h2>Upgrading FPP</h2>
<pre>
<?php
    echo "==================================================================================\n";
} else{
    echo "FPP Upgrade to version " . $version . "\n";
}

	$command = $SUDO . " " . escapeshellarg($fppDir . "/scripts/upgrade_FPP") . " " . escapeshellarg($version) . " 2>&1";

	echo "Command: $command\n";
	echo "----------------------------------------------------------------------------------\n";
    system($command);
	echo "\n";
if (!$wrapped) {
?>

==========================================================================
</pre>
<input id='RebootButton' type='button' value='Reboot' onClick='Reboot();'/>
</body>
</html>
<?
} else {
    echo "----------------------------------------------------------------------------------\n";
    echo "Upgrade complete.  Please reboot.\n";
}
?>
