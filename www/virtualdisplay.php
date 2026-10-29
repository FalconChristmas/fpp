<!DOCTYPE html>
<html lang="en">

<head>
	<?php
	include 'common/htmlMeta.inc';
	require_once 'config.php';
	require_once 'common.php';
	include 'common/menuHead.inc';


	// 16:9 default aspect but smaller by default
	$canvasWidth = 800;
	$canvasHeight = 450;

	if (isset($_GET['width'])) {
		$canvasWidth = (int) ($_GET['width']);
		$canvasHeight = (int) ($canvasWidth * 9.0 / 16.0);
	}
	?>
	<link rel="stylesheet" href="css/minimal.css?ref=<?php echo filemtime('css/minimal.css'); ?>" />

	<title>Virtual Display</title>
	<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, minimal-ui">
	<meta name="apple-mobile-web-app-capable" content="yes" />
</head>

<body>
	<h3>FPP Virtual Display</h3>
	<?
	require_once('virtualdisplaybody.php');
	?>

</body>

</html>