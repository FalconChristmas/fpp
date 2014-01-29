
<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<?php
    function piLCDenabledChecked()
    {
      if (ReadSettingFromFile("PI_LCD_Enabled") == false || 
          ReadSettingFromFile("PI_LCD_Enabled") == "false")
      {
        return "";
      }
      else
      {
        return " checked";
      }
    }
?>
<script type="text/javascript" src="/js/fpp.js"></script>
<script>
$(document).ready(function(){
  $("#chkPiLCDenabled").change(function(){
    var enabled = $("#chkPiLCDenabled").is(':checked')	
		var url = "fppxml.php?command=setPiLCDenabled&enabled=" + enabled;
    $.get(url,SetPiLCDenabled);
  });
});

function SetPiLCDenabled(data,status) 
{
  var i = 69;
  var i = 69;
}
</script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
<?php
//God save me for using a goto...
goto DEVELOPMENT;
?>
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
<?php
DEVELOPMENT:
?>

<FORM NAME="frmSettings">
<div id="global" class="settings">
<fieldset>
<legend>FPP Global Settings</legend>
  <table table width = "100%">
    <tr>
      <td width = "25%">PI 2x16 LCD Enabled:</td>
      <td width = "75%"><input type="checkbox" id="chkPiLCDenabled" value="1" <?php echo piLCDenabledChecked();?>></td>
    </tr>
  </table>

</fieldset>
</div>
</FORM>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
