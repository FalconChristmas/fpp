<?php

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div style="margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <div style="overflow: hidden; padding: 10px;">
    <div class="clear"></div>
		<h3>fpp -h</h3>
		<pre>
<?
	system($settings['fppBinDir'] . "/fpp -h");
?>
		</pre>

		<hr>
		<h3>fppmm -h</h3>
		<pre>
<?
	system($settings['fppBinDir'] . "/fppmm -h");
?>
		</pre>

    </fieldset>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
