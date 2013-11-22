<?php if ( !isset($_GET['nopage']) ): ?>
<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>

<?php
else:
require_once("config.php");
endif;

if ( file_exists($pluginDirectory."/".$_GET['plugin']."/".$_GET['page']) )

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
