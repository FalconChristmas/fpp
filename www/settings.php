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
<script>
$(document).ready(function(){
  $("#chkPiLCDenabled").change(function(){
    var enabled = $("#chkPiLCDenabled").is(':checked')	
		var url = "fppxml.php?command=setPiLCDenabled&enabled=" + enabled;
    $.get(url,SetPiLCDenabled);
  });

  $('#LogLevel').val(settings['LogLevel']);

  var logMasks = Array();

  if (typeof settings['LogMask'] !== 'undefined')
    logMasks = settings['LogMask'].split(",");

  for (var i = 0; i < logMasks.length; i++) {
    $('#LogMask input.mask_' + logMasks[i]).prop('checked', true);
  }
});

function SetPiLCDenabled(data,status) 
{
  var i = 69;
  var i = 69;
}

function LogLevelChanged()
{
	$.get("fppjson.php?command=setSetting&key=LogLevel&value="
		+ $('#LogLevel').val()).fail(function() { alert("Error saving new level") });
}

$(function() {
	$('#LogMask input').change(function() {
			var newValue = "";
			$('#LogMask input').each(function() {
					if ($(this).is(':checked', true)) {
						if (newValue != "") {
							newValue += ",";
						}
						newValue += $(this).attr('class').replace(/mask_/,"");
					}
				});

			$.get("fppjson.php?command=setSetting&key=LogMask&value="
				+ newValue).fail(function() { alert("Error saving new mask") });
		});
	});

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
      <td width = "25%">Blank screen on startup:</td>
      <td width = "75%"><? PrintSettingCheckbox("Screensaver", "screensaver", "1", "0"); ?></td>
    </tr>
    <tr>
      <td width = "25%">Pi 2x16 LCD Enabled:</td>
      <td width = "75%"><input type="checkbox" id="chkPiLCDenabled" value="1" <?php echo piLCDenabledChecked();?>></td>
    </tr>
    <tr>
      <td>FPPD Log Level:</td>
      <td><select id='LogLevel' onChange='LogLevelChanged();'>
            <option value='warn'>Warn</option>
            <option value='info'>Info</option>
            <option value='debug'>Debug</option>
            <option value='excess'>Excessive</option>
          </select></td>
    </tr>
    <tr>
      <td valign='top'>FPPD Log Mask:</td>
      <td>
        <table border=0 cellpadding=2 cellspacing=5 id='LogMask'>
          <tr>
            <td>Overrides</td>
            <td width='10px'></td>
            <td colspan=3 align=center>Log Areas</td>
            </tr>
          <tr>
            <td valign=top>
              <input type='checkbox' class='mask_all'>ALL<br>
              <input type='checkbox' class='mask_most'>Most
              </td>
            <td width='10px'></td>
            <td valign=top>
              <input type='checkbox' class='mask_channeldata'>Channel Data<br>
              <input type='checkbox' class='mask_channelout'>Channel Outputs<br>
              <input type='checkbox' class='mask_command'>Commands<br>
              <input type='checkbox' class='mask_control'>Control Interface<br>
              <input type='checkbox' class='mask_e131bridge'>E1.31 Bridge<br>
              <input type='checkbox' class='mask_effect'>Effects<br>
              <input type='checkbox' class='mask_event'>Events<br>
              </td>
            <td width='10px'></td>
            <td valign=top>
              <input type='checkbox' class='mask_general'>General<br>
              <input type='checkbox' class='mask_mediaout'>Media Outputs<br>
              <input type='checkbox' class='mask_playlist'>Playlists<br>
              <input type='checkbox' class='mask_schedule'>Scheduler<br>
              <input type='checkbox' class='mask_sequence'>Sequence Parser<br>
              <input type='checkbox' class='mask_setting'>Settings<br>
              <input type='checkbox' class='mask_sync'>Master/Slave Sync<br>
              </td>
          </tr>
        </table>
      </td>
    </tr>
  </table>

</fieldset>
</div>
</FORM>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
