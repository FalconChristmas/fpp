<?php

if ( !isset($_GET['nopage']) ):

require_once("config.php");

?>

<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
<?

$jsDir = $pluginDirectory . "/" . $_GET['plugin'] . "/js/";
if ( file_exists($jsDir))
{
	if ($handle = opendir($jsDir))
	{
		while (($file = readdir($handle)) !== false)
		{
			if (!in_array($file, array('.', '..')) && !is_dir($jsDir . $file))
			{
				printf( "<script type='text/javascript' src='/plugin.php?plugin=%s&file=js/%s&nopage=1'></script>\n",
					$_GET['plugin'], $file);
			}
		}
	}
}

$cssDir = $pluginDirectory . "/" . $_GET['plugin'] . "/css/";
if ( file_exists($cssDir))
{
	if ($handle = opendir($cssDir))
	{
		while (($file = readdir($handle)) !== false)
		{
			if (!in_array($file, array('.', '..')) && !is_dir($cssDir . $file))
			{
				printf( "<link rel='stylesheet' type='text/css' href='/plugin.php?plugin=%s&file=css/%s&nopage=1'>\n",
					$_GET['plugin'], $file);
			}
		}
	}
}

?>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>

<?php
else:
$skipJSsettings = 1;
require_once("config.php");
endif;

if ( !isset($_GET['plugin']) )
{
	echo "Please don't access this page directly";
}
elseif ( empty($_GET['plugin']) )
{
	echo "Plugin variable empty, please don't access this page directly";
}
elseif ( isset($_GET['page']) && !empty($_GET['page']) )
{
	if ( file_exists($pluginDirectory."/".$_GET['plugin']."/".$_GET['page']) )
	{
		-include_once($pluginDirectory."/".$_GET['plugin']."/".$_GET['page']);
	}
	else
	{
		echo "Error with plugin, requesting a page that doesn't exist";
	}
}
elseif ( isset($_GET['file']) && !empty($_GET['file']) )
{
	if (preg_match('/\.js$/', $_GET['file']))
		header('Content-type: text/javascript;');
	else if (preg_match('/\.css$/', $_GET['file']))
		header('Content-type: text/css;');
	else
		header('Content-type: text/plain;');


	$file = $pluginDirectory . "/" . $_GET['plugin'] . "/" . $_GET['file'];

	if (file_exists($file))
		readfile($file);
}
elseif ( file_exists($pluginDirectory."/".$_GET['plugin']."/plugin.php") )
{
	-include_once($pluginDirectory."/".$_GET['plugin']."/plugin.php");
}
else
{
	echo "Plugin invalid, no main page exists";
}

if ( !isset($_GET['nopage']) ): ?>

</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
<?php endif; ?>
