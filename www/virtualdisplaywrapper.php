<!DOCTYPE html>
<html>
<head>
<?php
require_once('common.php');
require_once('config.php');
include_once 'common/menuHead.inc';

?>

<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title>FPP Virtual Display</title>
</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br>
  <div>
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>FPP HTML Virtual Display</legend>
<?
require_once('virtualdisplaybody.php');
?>
	<br>NOTE: If the HTML Virtual Display is not working, you need to add the HTML Virtual Display Channel Output on the 'Other' tab of the Channel Outputs config page.  Your background image must be uploaded as a file called 'virtualdisplaybackground.jpg' and your pixel map must be copied to FPP's configuration directory from xLights as a file called 'virtualdisplaymap'.
    </fieldset>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
