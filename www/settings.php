<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<?php

$AlsaCards = Array();
exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
if ( $return_val )
{
	error_log("Error getting alsa cards for output!");
}
else
{
	foreach($output as $card)
	{
		$values = explode(':', $card);
		$AlsaCards[$values[1]] = $values[0];
	}
}
unset($output);

exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
if ( $return_val )
{
	error_log("Error getting currently selected alsa card used!");
}
else
{
	$CurrentCard = $output[0];
}
unset($output);

function PrintStorageDeviceSelect()
{
	global $SUDO;

	# FIXME, this would be much simpler by parsing "lsblk -l"
	exec('lsblk -l | grep /boot | cut -f1 -d" " | sed -e "s/p[0-9]$//"', $output, $return_val);
	$bootDevice = $output[0];
	unset($output);

	exec('lsblk -l | grep " /$" | cut -f1 -d" "', $output, $return_val);
	$rootDevice = $output[0];
	unset($output);

	exec('grep "fpp/media" /etc/fstab | cut -f1 -d" " | sed -e "s/\/dev\///"', $output, $return_val);
	$storageDevice = $output[0];
	unset($output);

	$found = 0;
	$values = Array();

	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^sd[a-z][0-9]/", $fileName)) ||
			(preg_match("/^mmcblk[0-9]p[0-9]/", $fileName)))
		{
			exec($SUDO . " sfdisk -s /dev/$fileName", $output, $return_val);
			$GB = intval($output[0]) / 1024.0 / 1024.0;
			unset($output);

			if ($GB <= 0.1)
				continue;

			$FreeGB = "Not Mounted";
			exec("df -k /dev/$fileName | grep $fileName | awk '{print $4}'", $output, $return_val);
			if (count($output))
			{
				$FreeGB = sprintf("%.1fGB Free", intval($output[0]) / 1024.0 / 1024.0);
				unset($output);
			}
			else
			{
				unset($output);

				if (preg_match("/^$rootDevice/", $fileName))
				{
					exec("df -k / | grep ' /$' | awk '{print \$4}'", $output, $return_val);
					if (count($output))
						$FreeGB = sprintf("%.1fGB Free", intval($output[0]) / 1024.0 / 1024.0);
					unset($output);
				}
			}

			$key = $fileName . " ";
			$type = "";

			if (preg_match("/^$bootDevice/", $fileName))
			{
				$type .= " (boot device)";
			}

			if (preg_match("/^sd/", $fileName))
			{
				$type .= " (USB)";
			}

			$key = sprintf( "%s - %.1fGB (%s) %s", $fileName, $GB, $FreeGB, $type);

			$values[$key] = $fileName;

			if ($storageDevice == $fileName)
				$found = 1;
		}
	}

	if (!$found)
	{
		$arr = array_reverse($values, true);
		$arr["-- Select a Storage Device --"] = "/dev/sda1";
		$values = array_reverse($arr);
	}

	PrintSettingSelect('StorageDevice', 'storageDevice', 0, 1, $storageDevice, $values);
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
  var logLevel = settings['LogLevel'];
  if (typeof logLevel === 'undefined')
    logLevel = "info";

  $('#LogLevel').val(logLevel);

  var logMasks = Array('most');

  if (typeof settings['LogMask'] !== 'undefined')
    logMasks = settings['LogMask'].split(",");

  for (var i = 0; i < logMasks.length; i++) {
    $('#mask_' + logMasks[i]).prop('checked', true);
  }
});

function LogLevelChanged()
{
	$.get("fppjson.php?command=setSetting&key=LogLevel&value="
		+ $('#LogLevel').val()).fail(function() { alert("Error saving new level") });
}

function SetAudio()
{
	$.get("fppjson.php?command=setAudioOutput&value="
		+ $('#AudioOutput').val()).fail(function() { alert("Failed to change audio output!") });
}

function ToggleLCDNow()
{
	var enabled = $('#PI_LCD_Enabled').is(":checked");
	$.get("fppxml.php?command=setPiLCDenabled&enabled="
		+ enabled).fail(function() { alert("Failed to enable LCD!") });
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
      <td width = "35%">Blank screen on startup:</td>
      <td width = "65%"><? PrintSettingCheckbox("Screensaver", "screensaver", 0, 1, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Force Analog Audio Output:</td>
      <td><? PrintSettingCheckbox("Force Analog Audio Output", "forceLocalAudio", 0, 0, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Pi 2x16 LCD Enabled:</td>
      <td><? PrintSettingCheckbox("Enable LCD Display", "PI_LCD_Enabled", 0, 0, "1", "0", "", "ToggleLCDNow"); ?></td>
    </tr>
    <tr>
      <td>Always transmit channel data:</td>
      <td><? PrintSettingCheckbox("Always Transmit", "alwaysTransmit", 1, 0, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Audio Output Device:</td>
      <td><? PrintSettingSelect("Audio Output Device", "AudioOutput", 1, 0, "$CurrentCard", $AlsaCards, "", "SetAudio"); ?></td>
    </tr>
    <tr>
      <td>External Storage Device:</td>
      <td><? PrintStorageDeviceSelect(); ?></td>
    </tr>
    <tr>
      <td>Log Level:</td>
      <td><select id='LogLevel' onChange='LogLevelChanged();'>
            <option value='warn'>Warn</option>
            <option value='info'>Info</option>
            <option value='debug'>Debug</option>
            <option value='excess'>Excessive</option>
          </select></td>
    </tr>
    <tr>
      <td valign='top'>Log Mask:</td>
      <td>
        <table border=0 cellpadding=2 cellspacing=5 id='LogMask'>
          <tr>
            <td valign=top>
              <input type='checkbox' id='mask_all' class='mask_all' onChange='MaskChanged(this);'>ALL<br>
              <br>
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
              <input type='checkbox' id='mask_most' class='mask_most' onChange='MaskChanged(this);'>Most (default)<br>
              <br>
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
<tr><td><a href="advancedsettings.php">Advanced Settings</a></td></tr>
  </table>

</fieldset>
</div>
</FORM>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
