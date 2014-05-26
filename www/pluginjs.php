<?php

// FIXME, maybe make a configjs.php to include where we want it and get rid of this var
$skipJSsettings = 1;
require_once("config.php");

header('Content-type: text/javascript;');

if (isset($_GET['plugin']) && isset($_GET['file']))
{
	$file = $pluginDirectory . "/" . $_GET['plugin'] . "/" . $_GET['file'];

	if (file_exists($file))
		readfile($file);
}

?>
