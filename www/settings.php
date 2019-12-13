<!DOCTYPE html>
<html>
<head>
<?php

require_once('common.php');
include 'common/menuHead.inc';

exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
if ( $return_val )
{
	error_log("Error getting currently selected alsa card used!");
}
else
{
	if (isset($output[0]))
		$CurrentCard = $output[0];
	else
		$CurrentCard = "0";
}
unset($output);

$AlsaCards = Array();
exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
if ( $return_val )
{
	error_log("Error getting alsa cards for output!");
}
else
{
	$foundOurCard = 0;
	foreach($output as $card)
	{
		$values = explode(':', $card);

		if ($values[0] == $CurrentCard)
			$foundOurCard = 1;

		if ($values[1] == "bcm2835 ALSA")
			$AlsaCards[$values[1] . " (Pi Onboard Audio)"] = $values[0];
		else if ($values[1] == "CD002")
			$AlsaCards[$values[1] . " (FM Transmitter)"] = $values[0];
		else
			$AlsaCards[$values[1]] = $values[0];
	}

	if (!$foundOurCard)
		$AlsaCards['-- Select an Audio Device --'] = $CurrentCard;
}
unset($output);

$AudioMixerDevice = "PCM";
if (isset($settings['AudioMixerDevice']))
{
	$AudioMixerDevice = $settings['AudioMixerDevice'];
}
else if ($settings['Platform'] == "BeagleBone Black")
{
	$AudioMixerDevice = exec($SUDO . " amixer -c $CurrentCard scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
    if ( $return_val )
    {
        $AudioMixerDevice = "PCM";
    }
}

$MixerDevices = Array();
exec($SUDO . " amixer -c $CurrentCard scontrols | cut -f2 -d\"'\"", $output, $return_val);
if ( $return_val || strpos($output[0], "Usage:") === 0) {
	error_log("Error getting mixer devices!");
    $AudioMixerDevice = "PCM";
} else {
	foreach($output as $device)
	{
		$MixerDevices[$device] = $device;
	}
}
unset($output);
    
$VideoOutputModels = Array();
if ($settings['Platform'] != "BeagleBone Black") {
    $VideoOutputModels['HDMI'] = "--HDMI--";
}
$VideoOutputModels['Disabled'] = "--Disabled--";
if (file_exists($settings['model-overlays'])) {
    $json = json_decode(file_get_contents($settings['model-overlays']));
    foreach ($json->models as $value) {
        $VideoOutputModels[$value->Name] = $value->Name;
    }
}

$backgroundColors = Array();
$backgroundColors['No Border']   = '';
$backgroundColors['Red']       = "ff0000";
$backgroundColors['Green']     = "008000";
$backgroundColors['Blue']      = "0000ff";
$backgroundColors['Aqua']      = "00ffff";
$backgroundColors['Black']     = "000000";
$backgroundColors['Gray']      = "808080";
$backgroundColors['Lime']      = "00FF00";
$backgroundColors['Navy']      = "000080";
$backgroundColors['Olive']     = "808000";
$backgroundColors['Purple']    = "800080";
$backgroundColors['Silver']    = "C0C0C0";
$backgroundColors['Teal']      = "008080";
$backgroundColors['White']     = "FFFFFF";
    
    
$ledTypes = Array();
    $ledTypes['Disabled'] = 0;
    $ledTypes['128x64 I2C (SSD1306)'] = 1;
    $ledTypes['128x64 Flipped I2C (SSD1306)'] = 2;
    $ledTypes['128x64 2 Color I2C (SSD1306)'] = 7;
    $ledTypes['128x64 2 Color Flipped I2C (SSD1306)'] = 8;
    $ledTypes['128x32 I2C (SSD1306)'] = 3;
    $ledTypes['128x32 Flipped I2C (SSD1306)'] = 4;
    $ledTypes['128x64 I2C (SH1106)'] = 5;
    $ledTypes['128x64 Flipped I2C (SH1106)'] = 6;
    $ledTypes['128x128 I2C (SSD1327)'] = 9;
    $ledTypes['128x128 Flipped I2C (SSD1327)'] = 10;
    
$AudioFormats = Array();
    $AudioFormats['Default'] = 0;
    $AudioFormats['44100/S16'] = 1;
    $AudioFormats['44100/S32'] = 2;
    $AudioFormats['44100/FLT'] = 3;
    $AudioFormats['48000/S16'] = 4;
    $AudioFormats['48000/S32'] = 5;
    $AudioFormats['48000/FLT'] = 6;
    $AudioFormats['96000/S16'] = 7;
    $AudioFormats['96000/S32'] = 8;
    $AudioFormats['96000/FLT'] = 9;
    
    
$BBBLeds = Array();
    $BBBLeds['Disabled'] = "none";
    $BBBLeds['Heartbeat'] = "heartbeat";
    $BBBLeds['SD Card Activity'] = "mmc0";
    $BBBLeds['eMMC Activity'] = "mmc1";
    $BBBLeds['CPU Activity'] = "cpu";
    
$BBBPowerLed = Array();
    $BBBPowerLed['Disabled'] = 0;
    $BBBPowerLed['Enabled'] = 1;

function PrintStorageDeviceSelect($platform)
{
	global $SUDO;

	# FIXME, this would be much simpler by parsing "lsblk -l"
	exec('lsblk -l | grep /boot | cut -f1 -d" " | sed -e "s/p[0-9]$//"', $output, $return_val);
    if (count($output) > 0) {
        $bootDevice = $output[0];
    } else {
        $bootDevice = "";
    }
	unset($output);

    if ($platform == "BeagleBone Black") {
        exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
        $rootDevice = $output[0];
        unset($output);
        
        if ($bootDevice == "") {
            exec('findmnt -n -o SOURCE / | colrm 1 5 | sed -e "s/p[0-9]$//"', $output, $return_val);
            $bootDevice = $output[0];
            unset($output);
        }
    } else {
        exec('lsblk -l | grep " /$" | cut -f1 -d" "', $output, $return_val);
        $rootDevice = $output[0];
        unset($output);
    }

	$storageDevice = "";
	exec('grep "fpp/media" /etc/fstab | cut -f1 -d" " | sed -e "s/\/dev\///"', $output, $return_val);
	if (isset($output[0]))
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
		$values = array_reverse($arr);
	}
    if ($storageDevice == "") {
        $storageDevice = $rootDevice;
    }

	PrintSettingSelect('StorageDevice', 'storageDevice', 0, 1, $storageDevice, $values, "", "", "checkFormatStorage");
}

?>


<script type="text/javascript" src="jquery/jQuery.msgBox/scripts/jquery.msgBox.js"></script>
<link href="jquery/jQuery.msgBox/styles/msgBoxLight.css" rel="stylesheet" type="text/css">
<script>

function checkForStorageCopy() {
    $.msgBox({
             title: "Copy settings?",
             content: "Would you like to copy all files to the new storage location?\nAll settings on the new storage will be overwritten.",
             type: "info",
             buttons: [{ value: "Yes" }, { value: "No" }],
             success: function (result) {
                 storageDeviceChanged();
                 if (result == "Yes") {
                    window.location.href="copystorage.php?storageLocation=" + $('#storageDevice').val();
                 }
            }
        });
}

function checkFormatStorage()
{
    var value = $('#storageDevice').val();
    
    var e = document.getElementById("storageDevice");
    var name = e.options[e.selectedIndex].text;
    if (name.includes("Not Mounted")) {
        var btitle = "Format Storage Location (" + value + ")" + name;
        $.msgBox({ type: "prompt",
                 title: btitle,
                 inputs: [
                 { header: "Don't Format", type: "radio", name: "formatType", checked:"", value: "none" },
                 { header: "FAT (Compatible with Windows/OSX)", type: "radio", name: "formatType", value: "FAT"},
                 { header: "ext4 (Most stable)", type: "radio", name: "formatType", value: "ext4" },
                 { header: "btrfs (Compression, Fastest)", type: "radio", name: "formatType", value: "btrfs" }],
                 buttons: [ { value: "OK" } ],
                 opacity: 0.5,
                 success: function (result, values) {
                 var v = $('input[name=formatType]:checked').val();
                 if (v != "none") {
                    $.ajax({ url: "formatstorage.php?fs=" + v + "&storageLocation=" + $('#storageDevice').val(),
                        async: false,
                        success: function(data) {
                           checkForStorageCopy();
                        },
                        failure: function(data) {
                        DialogError("Formate Storage", "Error formatting storage.");
                        }
                        });
                    } else {
                        checkForStorageCopy();
                    }
                 }
                 });
    } else {
        storageDeviceChanged();
    }
}

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
	$.get("fppjson.php?command=setSetting&key=AudioOutput&value="
		+ $('#AudioOutput').val()).fail(function() { alert("Failed to change audio output!") });
    location.reload();
}

function SetMixerDevice()
{
	$.get("fppjson.php?command=setSetting&key=AudioMixerDevice&value="
		+ $('#AudioMixerDevice').val()).fail(function() { alert("Failed to change audio output!") });
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
<?php
    if ($settings['Platform'] == "Raspberry Pi") {
?>
    <tr>
      <td width = "45%">Blank screen on startup:</td>
      <td width = "55%"><? PrintSettingCheckbox("Screensaver", "screensaver", 0, 1, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Force HDMI Display:</td>
      <td><? PrintSettingCheckbox("Force HDMI Display", "ForceHDMI", 0, 1, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Force Legacy audio outputs (mpg123/ogg123):</td>
      <td><? PrintSettingCheckbox("Force Legacy Audio Outputs", "LegacyMediaOutputs", 0, 0, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Pi 2x16 LCD Enabled:</td>
      <td><? PrintSettingCheckbox("Enable LCD Display", "PI_LCD_Enabled", 0, 0, "1", "0", "", "ToggleLCDNow"); ?></td>
    </tr>
<?php
}
?>
    <tr>
      <td>Always transmit channel data:</td>
      <td><? PrintSettingCheckbox("Always Transmit", "alwaysTransmit", 2, 0, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Blank between sequences:</td>
      <td><? PrintSettingCheckbox("Blank Between Sequences", "blankBetweenSequences", 2, 0, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Pause Background Effect Sequence when playing a FSEQ file:</td>
      <td><? PrintSettingCheckbox("Pause Background Effects", "pauseBackgroundEffects", 2, 0, "1", "0"); ?></td>
    </tr>
    <tr>
      <td>Default Video Output Device:</td>
<td><? PrintSettingSelect("Video Output Device", "VideoOutput", 0, 0, isset($settings['videoOutput']) ? $settings['videoOutput'] : array_values($VideoOutputModels)[0], $VideoOutputModels); ?></td>
    </tr>
<?php
 if ($settings['Platform'] == "Raspberry Pi") {
?>
    <tr>
      <td>OMXPlayer (mp4 playback) Audio Output:</td>
      <td><? PrintSettingSelect("OMXPlayer Audio Device", "OMXPlayerAudioOutput", 0, 0, isset($settings['OMXPlayerAudioOutput']) ? $settings['OMXPlayerAudioOutput'] : "0",
                                Array("ALSA" => "alsa", "HDMI" => "hdmi", "Local" => "local", "Both" => "both", "Disabled" => "disabled")); ?>
     </td>
    </tr>
    <tr>
      <td>Disable IP announcement during boot:</td>
      <td><? PrintSettingCheckbox("Disable IP announcement during boot", "disableIPAnnouncement", 0, 0, "1", "0"); ?></td>
    </tr>
<?php
 }
?>
    <tr>
      <td>Audio Output Device:</td>
      <td><? PrintSettingSelect("Audio Output Device", "AudioOutput", 2, 0, "$CurrentCard", $AlsaCards, "", "SetAudio"); ?></td>
    </tr>
    <tr>
      <td>Audio Output Mixer Device:</td>
      <td><? PrintSettingSelect("Audio Mixer Device", "AudioMixerDevice", 2, 0, $AudioMixerDevice, $MixerDevices, "", "SetMixerDevice"); ?></td>
    </tr>
    <tr>
      <td>Audio Output Format:</td>
      <td><? PrintSettingSelect("Audio Output Format", "AudioFormat", 2, 0, isset($settings['AudioFormat']) ? $settings['AudioFormat'] : "0", $AudioFormats); ?></td>
    </tr>
    <tr>
      <td>UI Border Color:</td>
      <td><? PrintSettingSelect("UI Background Color", "backgroundColor", 0, 0, isset($settings['backgroundColor']) ? $settings['backgroundColor'] : "", $backgroundColors, "", "reloadPage"); ?></td>
    </tr>
<? if ($settings['SubPlatform'] != "Docker" ) { ?>
    <tr>
      <td>Storage Device:</td>
      <td><? PrintStorageDeviceSelect($settings['Platform']); ?></td>
    </tr>
<? }
   if ( file_exists("/dev/i2c-1") || file_exists("/dev/i2c-2") ) { ?>
    <tr>
        <td>OLED Status Display:</td>
        <td><? PrintSettingSelect("OLED Status Display", "LEDDisplayType", 0, 1, isset($settings['LEDDisplayType']) ? $settings['LEDDisplayType'] : "", $ledTypes); ?>
        </td>
    </tr>
<? } ?>

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
<?php
    if ($settings['Platform'] == "BeagleBone Black") {
?>
    <tr>
        <td valign='top'>BeagleBone LEDs:</td>
        <td>
            <table border=0 cellpadding=0 cellspacing=5 id='BBBLeds'>
                <tr><td valign=top>USR1:</td><td><? PrintSettingSelect("USR1 LED", "BBBLeds0", 0, 0, isset($settings['BBBLeds0']) ? $settings['BBBLeds0'] : "heartbeat", $BBBLeds); ?></td></tr>
                <tr><td valign=top>USR2:</td><td><? PrintSettingSelect("USR2 LED", "BBBLeds1", 0, 0, isset($settings['BBBLeds1']) ? $settings['BBBLeds1'] : "mmc0", $BBBLeds); ?></td></tr>
                <tr><td valign=top>USR3:</td><td><? PrintSettingSelect("USR3 LED", "BBBLeds2", 0, 0, isset($settings['BBBLeds2']) ? $settings['BBBLeds2'] : "cpu", $BBBLeds); ?></td></tr>
                <tr><td valign=top>USR4:</td><td><? PrintSettingSelect("USR4 LED", "BBBLeds3", 0, 0, isset($settings['BBBLeds3']) ? $settings['BBBLeds3'] : "mmc1", $BBBLeds); ?></td></tr>
                <tr><td valign=top>Power:</td><td><? PrintSettingSelect("Power LED", "BBBLedPWR", 0, 0, isset($settings['BBBLedPWR']) ? $settings['BBBLedPWR'] : 1, $BBBPowerLed); ?></td></tr>
            </table>
        </td>
    </tr>
<? } ?>

<tr><td><a href="advancedsettings.php">Advanced Settings</a></td></tr>
  </table>

</fieldset>
</div>
</FORM>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
