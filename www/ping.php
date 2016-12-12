<?php

$skipJSsettings = 1;
require_once('auth.php');
require_once("common.php");

DisableOutputBuffering();

$ip = "github.com";
$count = 5;

if (isset($_GET['ip']))
	$ip = $_GET['ip'];

if (isset($_GET['count']))
	$count = $_GET['count'];

$ip = preg_replace('/[^-a-z0-9\.]/', '', $ip);
?>

<html>
<head>
<title>
Ping
</title>
</head>
<body>
<h2>Ping <? echo $ip; ?></h2>
<pre>
==========================================================================
<?php
	system("ping -c $count -n $ip");
	#echo "==================================================================================\n";
	#system("traceroute -n $ip");
?>
==========================================================================
</pre>
</body>
</html>
