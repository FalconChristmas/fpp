<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<?php
    function getAlsaCards()
    {
      exec("for card in /proc/asound/card*/id; do echo -n \$card | sed 's/.*card\\([0-9]*\\).*/\\1:/g'; cat \$card; done", $output, $return_val);
      if ( $return_val )
      {
        error_log("Error getting alsa cards for output!");
        return;
      }

      $cards = Array();
      foreach($output as $card)
      {
        $values = explode(':', $card);
        $cards[$values[0]] = $values[1];
      }

      return $cards;
    }

    function getAudioOutput()
    {
      $current_card = trim(exec(SUDO . " sed -n '/card [0-9]/s/card \\([0-9]*\\)/\\1/p' /root/.asoundrc", $output, $return_val));
      if ($return_val)
      {
        error_log("Failed to get current audio output!");
        return;
      }
      return $current_card;
    }

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

  var logLevel = settings['LogLevel'];
  if (typeof logLevel === 'undefined')
    logLevel = "info";

  $('#LogLevel').val(logLevel);
  $('#AudioOutput').val(<?php echo getAudioOutput(); ?>);

  var logMasks = Array('most');

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

function AudioOutputChanged()
{
	$.get("fppjson.php?command=setAudioOutput&value="
		+ $('#AudioOutput').val()).fail(function() { alert("Failed to change audio output!") });
}

</script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
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
              <input type='checkbox' class='mask_sync'>MultiSync<br>
              </td>
          </tr>
        </table>
      </td>
    </tr>
<tr>
<td>
Audio Output Device:
</td>
<td>
<select id='AudioOutput' onChange='AudioOutputChanged();'>

<?php
foreach ( getAlsaCards() as $audio_val => $audio_key )
	echo "<option value='".$audio_val."'>".$audio_key."</option>";
?>

</select>
</td>
</tr>
  </table>

</fieldset>
</div>
</FORM>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
