<?php
if ( isset($_POST['submit']) )
{
	echo "<html><body>We don't do anything yet, sorry!</body></html>";
	exit(0);
}
?>

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
<FORM NAME="password_form" ACTION="<?php echo $_SERVER['PHP_SELF'] ?>" METHOD="POST">

<br />

<div id="usb" class="settings">
<fieldset>
<legend>USB Devices</legend>
<?php
$devices=explode("\n",trim(shell_exec("ls -w 1 /dev/ttyUSB*")));
foreach ($devices as $device)
{
	echo "WE FOUND DEVICE $device... what is it used for (renard, dmx, rds?\n";
	echo "<br />\n";
}
?>
</fieldset>
</div>

<br />

<div id="global" class="settings">
<fieldset>
<legend>FPP Global Settings</legend>
pixelnet?
e1.31?
</fieldset>
</div>

<br />


</div>
     </FORM>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
