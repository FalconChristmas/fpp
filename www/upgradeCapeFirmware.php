<!DOCTYPE html>
<html>
<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();
?>

<head>
<title>
Upgrading Cape Firmware
</title>
<script type="text/javascript" src="js/jquery-latest.min.js"></script>
<script type="text/javascript" src="js/jquery-ui.min.js"></script>
<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
</head>
<body>
<h2>Upgrading Cape Firmware</h2>
<pre>
<?php
    $file = $_FILES["firmware"]["tmp_name"];
    
    system("sudo /opt/fpp/scripts/upgradeCapeFirmware " . $file);

    copy($file, "/tmp/doo.bin");
    unlink($file);
    WriteSettingToFile('rebootFlag', 1);
?>

==========================================================================
</pre>
</body>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>

</html>
