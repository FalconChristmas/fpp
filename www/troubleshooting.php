<!DOCTYPE html>
<html>
<?php
require_once('troubleshootingCommands.php');
?>

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
      <legend>Troubleshooting Commands</legend>
      <div style="overflow: hidden; padding: 10px;">
    <div class="clear"></div>
<?
foreach ($commands as $title => $command)
{
?>
				<h3><? echo $title . ':&nbsp;&nbsp;&nbsp;&nbsp;' . $command; ?></h3><pre><? echo $results[$command]; ?></pre><hr>
<?
}
?>
    </fieldset>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
