<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<title><? echo $pageTitle; ?></title>

<script type="text/javascript" src="jquery/jQuery.msgBox/scripts/jquery.msgBox.js"></script>
<link href="jquery/jQuery.msgBox/styles/msgBoxLight.css" rel="stylesheet" type="text/css">

<script>
function growSDCardFS() {
    $('#dialog-confirm')
        .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
            "Yes" : function() {
                $(this).dialog("close");
                window.location.href="growsd.php";
                SetRebootFlag();
            },
            "No" : function() {
                $(this).dialog("close");
             }
        }
    });
}
function newSDCardPartition() {
    $('#dialog-confirm-newpartition')
    .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
            "Yes" : function() {
            $(this).dialog("close");
            window.location.href="newpartitionsd.php";
            SetRebootFlag();
            },
            "No" : function() {
            $(this).dialog("close");
            }
            }
            });
}

<?php
    if ($settings['Platform'] == "BeagleBone Black") {
?>

function flashEMMC() {
    $('#dialog-confirm-emmc')
        .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
                "Yes" : function() {
                $(this).dialog("close");
                window.location.href="flashbbbemmc.php";
            },
            "No" : function() {
                $(this).dialog("close");
            }
        }
    });
}
function flashEMMCBtrfs() {
    $('#dialog-confirm-emmc')
    .dialog({
            resizeable: false,
            height: 300,
            width: 500,
            modal: true,
            buttons: {
            "Yes" : function() {
            $(this).dialog("close");
            window.location.href="flashbbbemmc-btrfs.php";
            },
            "No" : function() {
            $(this).dialog("close");
            }
            }
            });
}
<?php
    }
    
    
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
                     { header: "ext4 (Most stable)", type: "radio", name: "formatType", value: "ext4" },
                     { header: "FAT (Compatible with Windows/OSX, unsupported, slow)", type: "radio", name: "formatType", value: "FAT"}
                 ],
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


</script>

</head>
<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="global" class="settings">
	<fieldset>
	<legend>FPP Advanced Settings</legend>
	<table table width = "100%">
        <? if ($settings['SubPlatform'] != "Docker" ) { ?>
        <tr>
          <td><b>Storage Device:</b></td>
          <td><? PrintStorageDeviceSelect($settings['Platform']); ?><br>
            Changing the storage device to anything other than the SD card is NOT supported and untested.  If it fails to work, your system may not boot properly.  Using a partition type other than ext4 is also not supported and may cause playback issues.  If you are not extremely comfortable with Linux and diagnosing issues, do not change this.  This option will likely be removed in a future version of FPP. 
            </td>
        </tr>
        <tr><td colspan='2'><hr></td></tr>
    <? } ?>
        <tr><td valign='top' align="right"><? PrintSettingCheckbox("Show All Options", "showAllOptions", 0, 0, "1", "0"); ?></td><td><b>Display all options/settings</b> -
If
                turned off and FPPD can detect what hardware (cape/hat/etc...) is connected, certain options that are either incompatible with the
                hardware or are rarely used may not be displayed.</td></tr>
        <tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingSelect("E1.31 Bridging Transmit Interval", "E131BridgingInterval", 2, 0, "50", Array('10ms' => '10', '25ms' => '25', '40ms' => '40', '50ms' => '50', '100ms' => '100')); ?></td>
			<td valign='top'><b>E1.31 Bridge Mode Transmit Interval</b> - The
				default Transmit Interval in E1.31 Bridge Mode is 50ms.  This
				setting allows changing this to match the rate the player is
				outputting. <font color='#ff0000'><b>WARNING</b></font> - Some
				output devices such as the FPD do not support rates other than 50ms.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingSelect("Boot Delay", "bootDelay", 0, 0, "0", Array('0s' => '0', '1s' => '1', '2s' => '2', '3s' => '3', '4s' => '4', '5s' => '5', '6s' => '6', '7s' => '7', '8s' => '8', '9s' => '9', '10s' => '10', '15s' => '10', '20s' => '20', '25s' => '25', '30s' => '30')); ?></td>
			<td valign='top'><b>Boot Delay</b> - The time that FPP waits after
				system boot up to start fppd.  For environments that are
				powered down regularly, fppd may start up quicker than the
				network environment fully starts up which may cause E1.31
				multicast to not work properly.  Setting a Boot Delay will
				cause fppd to wait 'X' number of seconds to start which can
				give the network switches and routers time to fully start up.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
<?
	if ($settings['fppMode'] != 'remote') {
?>
		<tr><td valign='top'><? PrintSettingTextSaved("mediaOffset", 2, 0, 5, 5, "", "0"); ?> ms</td>
			<td valign='top'><b>Media/Sequence Offset</b> - The Media Offset value
				allows adjusting the synchronization of the media and sequences being
				played.  The value is specified in milliseconds.  A positive value
				moves the media ahead, a negative value moves the media back.
				Changing this value requires a FPPD restart.
				<font color='#ff0000'><b>WARNING</b></font> - This offset applies
				to all media files played.  If your media files require different
				offsets per file then you will have to edit the audio files or
				sequences to bring them into sync.</td>
		</tr>
<?
        if ($settings['fppMode'] == 'master') {
?>
        <tr><td valign='top'><? PrintSettingTextSaved("openStartDelay", 2, 0, 5, 5, "", "0"); ?> ms</td>
			<td valign='top'><b>Open/Start Delay</b> - An extra delay (in ms) between the master sending
            the "Open" command and actually starting the sequence.  This can be used to allow the remotes
            to have extra time to open the sequence, process videos, etc...   This requires master and
            remote to both be running FPP 3.2 or newer.  For complext MP4 files on remotes, a
            value of around 650-800 is likely needed for the remote video to be loaded and buffers filled
            prior to first frame being displayed.</td>
		</tr>
<?
        }
	} else {
?>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingTextSaved("remoteOffset", 2, 0, 5, 5, "", "0"); ?> ms</td>
			<td valign='top'><b>Remote Media/Sequence Offset</b> - The Remote Offset value
				allows adjusting the synchronization of a FPP Remote.
				The value is specified in milliseconds.  A positive value
				moves the remove ahead, a negative value moves the remote back.
				Changing this value requires a FPPD restart on the Remote.
				<font color='#ff0000'><b>WARNING</b></font> - This offset applies
				to both sequence and media files.  If your media files require different
				offsets per file then you will need to edit the media files
				to bring them into sync.</td>
		</tr>
<?php
        if ($settings['Platform'] == "Raspberry Pi") {
?>
            <tr><td valign='top' align="right"><? PrintSettingCheckbox("Ignore media sync packets", "remoteIgnoreSync", 2, 0, "1", "0") ?></td>
                <td valign='top'><b>Ignore Media Sync Packets</b> - When enabled, videos played with omxplayer on the remote will be started and stopped
                when the master sends the start/stop events, but no attempt will be made to keep the video in sync with the
                master during playback.  The video will run smoother, but may get out of sync with the master.</td>
            </tr>
<?
        }
    }

    $addnewfsbutton = false;
    $addflashbutton = false;
    exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
    $rootDevice = $output[0];
    if ($rootDevice == 'mmcblk0p1' || $rootDevice == 'mmcblk0p2') {
        if (isset($settings["LastBlock"]) && $settings['LastBlock'] < 7200000) {
            $addnewfsbutton = true;
        }
        if ($settings['Platform'] == "BeagleBone Black") {
            if (strpos($settings['SubPlatform'], 'PocketBeagle') === FALSE) {
                $addflashbutton = true;
            }
        }
    }
    if ($addnewfsbutton) {
?>
        <tr><td colspan='2'><hr></td></tr>
        <tr><td>
        <input type='button' class='buttons' value='Grow Filesystem' onClick='growSDCardFS();'>
        </td>
        <td><b>Grow file system on SD card</b> - This will grow the file system on the SD card to use
        the entire size of the SD card.</td>
        </tr>
        <tr><td colspan='2'><hr></td></tr>
        <tr><td>
        <input type='button' class='buttons' value='New Partition' onClick='newSDCardPartition();'>
        </td>
        <td><b>New partition on SD card</b> - This will create a new partition in the unused aread of the SD card.  The new partition can be selected as a storage location and formatted to BTRFS or ext4 after a reboot.</td>
        </tr>
<?php
	}
    if ($addflashbutton) {
?>
            <tr><td colspan='2'><hr></td></tr>
            <tr><td>
                <input type='button' class='buttons' value='Flash to eMMC' onClick='flashEMMC();'>
                </td>
                <td><b>Flash to eMMC</b> - This will copy FPP to the internal eMMC.</td>
            </tr>
            <tr><td colspan='2'><hr></td></tr>
            <tr><td>
                <input type='button' class='buttons' value='Flash to eMMC' onClick='flashEMMCBtrfs();'>
                </td>
<td><b>Flash to eMMC - BTRFS root</b> - This will copy FPP to the internal eMMC, but use BTRFS for the root filesystem.  BTRFS uses compression to save a lot of space on the eMMC, but at the expense of extra CPU usage.</td>
            </tr>
<?php
    }
?>
	</table>
	</fieldset>
<br/>
    <fieldset>
          <legend>MQTT</legend>
            <table  border="0" cellpadding="1" cellspacing="1">
                <tr>
                    <td >MQTT Broker Host:</td>
                    <td ><? PrintSettingTextSaved("MQTTHost", 2, 0, 64, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Broker Port:</td>
                    <td ><? PrintSettingTextSaved("MQTTPort", 2, 0, 32, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Prefix:</td>
                    <td ><? PrintSettingTextSaved("MQTTPrefix", 2, 0, 32, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Username:</td>
                    <td ><? PrintSettingTextSaved("MQTTUsername", 2, 0, 32, 32, "", ""); ?></td>
		</tr>
                <tr>
                    <td >MQTT Password:</td>
                    <td ><? PrintSettingPasswordSaved("MQTTPassword", 2, 0, 32, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >CA File (Optional):</td>
		    <td ><? PrintSettingTextSaved("MQTTCaFile", 2, 0, 64, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Publish Frequency (Optional):</td>
		    <td ><? PrintSettingTextSaved("MQTTFrequency", 2, 0, 8, 8, "", "0"); ?></td>
                </tr>
	    </table>
	    MQTT events will be published to "$prefix/falcon/player/$hostname/" with playlist events being in the "playlist" subtopic. <br/>
	    CA file is the full path to the signer certificate.  Only needed if using mqtts server that is self signed.<br/>
	    Publish Frequence should be zero (disabled) or the number of seconds between periodic mqtt publish events<br/><br/>
FPP will respond to certain events:
<div class="fppTableWrapper">
<table width = "100%" border="0" cellpadding="1" cellspacing="1">
<tr><th>Topic</th><th>Action</th></tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/start</td><td>Starts the playlist (optional payload can be index of item to start with)</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/next</td><td>Forces playing of the next item in the playlist (payload ignored)</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/prev</td><td>Forces playing of the previous item in the playlist (payload ignored)</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/repeat</td><td>If payload is "1", will turn on repeat, otherwise it is turned off</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/sectionPosition</td><td>Payload contains an integer for the position in the playlist (0 based)</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/stop/now</td><td>Forces the playlist to stop immediately.  PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/stop/graceful</td><td>Gracefully stop playlist.  PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/set/playlist/${PLAYLISTNAME}/stop/afterloop</td><td>Allow playlist to finish current loop then stop.  PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/event/</td><td>Starts the event identified by the payload.   The payload format is MAJ_MIN identifying the event.</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/effect/start</td><td>Starts the effect named in the payload</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/effect/stop</td><td>Stops the effect named in the payload or all effects if payload is empty</td>
</tr>
</table>
</div>
    </fieldset>
</div>
<div id="dialog-confirm" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Growing the filesystem will sometimes require a reboot to take effect.  Do you wish to proceed?</p>
</div>
<div id="dialog-confirm-emmc" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Flashing the eMMC can take a long time.  Do you wish to proceed?</p>
</div>
<div id="dialog-confirm-newpartition" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Creating a new partition in the unused space will require a reboot to take effect.  Do you wish to proceed?</p>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
