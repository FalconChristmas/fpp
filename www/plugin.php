<?php

if ( !isset($_GET['nopage']) ):

require_once("config.php");
require_once("common.php");

$pluginSettings = array();

if (isset($_GET['plugin']))
{
	$pluginConfigFile = $settings['configDirectory'] . "/plugin." . $_GET['plugin'];
	if (file_exists($pluginConfigFile))
		$pluginSettings = parse_ini_file($pluginConfigFile);
}

?>

<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<title><? echo $pageTitle; ?></title>
<script type="text/javascript">
	var pluginSettings = new Array();
<?
	foreach ($pluginSettings as $key => $value) {
		printf("	pluginSettings['%s'] = \"%s\";\n", $key, $value);
	}
?>
</script>

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
	$file = $pluginDirectory . "/" . $_GET['plugin'] . "/" . $_GET['file'];

	if (file_exists($file))
	{
		$filename = basename($file);
		$file_extension = strtolower(substr(strrchr($filename,"."),1));

		switch( $file_extension ) {
			case "gif": $ctype="image/gif;"; break;
			case "png": $ctype="image/png;"; break;
			case "jpeg":
			case "jpg": $ctype="image/jpg;"; break;
			case "js":  $ctype="text/javascript;"; break;
			case "css": $ctype="text/css;"; break;
			default:    $ctype="text/plain;"; break;
		}

		header('Content-type: ' . $ctype);

		// Without the clean/flush we send two extra bytes that
		// cause the image to be corrupt.  This is similar to the
		// bug we had with an extra 2 bytes in our log zip
		ob_clean();
		flush();
		readfile($file);
		exit();
	}
	else
	{
		error_log("Error, could not find file $file");
		echo "Error with plugin, requesting a file that doesn't exist";
	}
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
</div>
</body>
</html>
<?php endif; ?>
