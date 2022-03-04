<?
header( "Access-Control-Allow-Origin: *");

$wrapped = 1;
$version = escapeshellcmd($_GET['version']);

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
<script type="text/javascript" src="js/jquery-latest.min.js"></script>
<script type="text/javascript" src="js/jquery-ui.min.js"></script>
<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
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

	$command = "sudo " . $fppDir . "/scripts/upgrade_FPP " . $version . " 2>&1";

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
