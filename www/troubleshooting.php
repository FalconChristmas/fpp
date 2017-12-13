<?php
require_once('troubleshootingCommands.php');
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
