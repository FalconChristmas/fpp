<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');

// 16:9 default aspect but smaller by default
$canvasWidth = 800;
$canvasHeight = 450;

if (isset($_GET['width']))
{
	$canvasWidth = $_GET['width'];
	$canvasHeight = (int)($canvasWidth * 9.0 / 16.0);
}
?>
<script type="text/javascript" src="js/jquery-3.4.1.min.js"></script>
<script type="text/javascript" src="js/jquery-migrate-3.1.0.min.js"></script>
<script type="text/javascript" src="js/jquery-ui.min.js"></script>
<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
<link rel="stylesheet" href="css/jquery-ui.css" />
<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />
<link rel="stylesheet" href="css/minimal.css?ref=<?php echo filemtime('css/minimal.css'); ?>" />
<script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>

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
