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

	var queuedChanges = 0;
	function MaskChanged(cbox)
	{
		var newValue = false;
		if ($(cbox).is(':checked', true))
			newValue = true;

		var id = $(cbox).attr('id');
		if (id == "mask_all")
		{
			$('#LogMask input').prop('checked', false);
			// Do this so we don't have to put class='mask_most,mask_all' on every 'most' items
			$('#LogMask input.mask_most').prop('checked', newValue);
			$('#LogMask input.mask_all').prop('checked', newValue);
			$('#mask_most').prop('checked', false);
		}
		else if (id == "mask_most")
		{
			$('#LogMask input').prop('checked', false);
			$('#LogMask input.mask_most').prop('checked', newValue);
		}
		else
		{
			$('#mask_most').prop('checked', false);
			$('#mask_all').prop('checked', false);
		}

		var newValue = "";
		$('#LogMask input').each(function() {
				if ($(this).is(':checked', true)) {
					if (newValue != "") {
						newValue += ",";
					}
					newValue += $(this).attr('id').replace(/(.*,)?mask_/,"");
				}
			});

		$.get("fppjson.php?command=setSetting&key=LogMask&value="
			+ newValue).fail(function() { alert("Error saving new mask") });

		$.ajax({ url: 'fppjson.php?command=setSetting&key=LogMask&value=' + newValue, 
				async: false,
				dataType: 'json',
				success: function(data) {
					$.jGrowl("Log Mask Saved.");
				},
				failure: function(data) {
					DialogError("Save Log Mask", "Error Saving new Log Mask.");
				}
		});
	}

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
    $('#mask_' + logMasks[i]).prop('checked', true);
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

function AudioOutputChanged()
{
	$.get("fppjson.php?command=setAudioOutput&value="
		+ $('#AudioOutput').val()).fail(function() { alert("Failed to change audio output!") });
}

</script>
<title><? echo $pageTitle; ?></title>
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
      <td width = "25%">Force Analog Audio Output:</td>
      <td width = "75%"><? PrintSettingCheckbox("Force Analog Audio Output", "forceLocalAudio", "1", "0"); ?></td>
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
              <input type='checkbox' id='mask_all' class='mask_all' onChange='MaskChanged(this);'>ALL<br>
              <input type='checkbox' id='mask_most' class='mask_most' onChange='MaskChanged(this);'>Most
              </td>
            <td width='10px'></td>
            <td valign=top>
              <input type='checkbox' id='mask_channeldata' class='mask_all' onChange='MaskChanged(this);'>Channel Data<br>
              <input type='checkbox' id='mask_channelout' class='mask_most' onChange='MaskChanged(this);'>Channel Outputs<br>
              <input type='checkbox' id='mask_command' class='mask_most' onChange='MaskChanged(this);'>Commands<br>
              <input type='checkbox' id='mask_control' class='mask_most' onChange='MaskChanged(this);'>Control Interface<br>
              <input type='checkbox' id='mask_e131bridge' class='mask_most' onChange='MaskChanged(this);'>E1.31 Bridge<br>
              <input type='checkbox' id='mask_effect' class='mask_most' onChange='MaskChanged(this);'>Effects<br>
              <input type='checkbox' id='mask_event' class='mask_most' onChange='MaskChanged(this);'>Events<br>
              <input type='checkbox' id='mask_general' class='mask_most' onChange='MaskChanged(this);'>General<br>
              </td>
            <td width='10px'></td>
            <td valign=top>
              <input type='checkbox' id='mask_gpio' class='mask_most' onChange='MaskChanged(this);'>GPIO<br>
              <input type='checkbox' id='mask_mediaout' class='mask_most' onChange='MaskChanged(this);'>Media Outputs<br>
              <input type='checkbox' id='mask_sync' class='mask_most' onChange='MaskChanged(this);'>MultiSync<br>
              <input type='checkbox' id='mask_playlist' class='mask_most' onChange='MaskChanged(this);'>Playlists<br>
              <input type='checkbox' id='mask_plugin' class='mask_most' onChange='MaskChanged(this);'>Plugins<br>
              <input type='checkbox' id='mask_schedule' class='mask_most' onChange='MaskChanged(this);'>Scheduler<br>
              <input type='checkbox' id='mask_sequence' class='mask_most' onChange='MaskChanged(this);'>Sequence Parser<br>
              <input type='checkbox' id='mask_setting' class='mask_most' onChange='MaskChanged(this);'>Settings<br>
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
