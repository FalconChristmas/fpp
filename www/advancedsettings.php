<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<title><? echo $pageTitle; ?></title>

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
?>
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
		<tr><td valign='top'><? PrintSettingSelect("E1.31 Bridging Transmit Interval", "E131BridgingInterval", 1, 0, "50", Array('10ms' => '10', '25ms' => '25', '40ms' => '40', '50ms' => '50', '100ms' => '100')); ?></td>
			<td valign='top'><b>E1.31 Bridge Mode Transmit Interval</b> - The
				default Transmit Interval in E1.31 Bridge Mode is 50ms.  This
				setting allows changing this to match the rate the player is
				outputting. <font color='#ff0000'><b>WARNING</b></font> - Some
				output devices such as the FPD do not support rates other than 50ms.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
<?
	if ($settings['fppMode'] != 'remote')
	{
?>
		<tr><td valign='top'><? PrintSettingText("mediaOffset", 1, 0, 5, 5); ?> ms<br>
				<? PrintSettingSave("Media Offset", "mediaOffset", 1, 0); ?></td>
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
	}
	else
	{
?>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingText("remoteOffset", 1, 0, 5, 5); ?> ms<br>
				<? PrintSettingSave("Remote Offset", "remoteOffset", 1, 0); ?></td>
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
<?
    }
if (isset($settings["LastBlock"]) && $settings['LastBlock'] < 7000000) {
    $addnewfsbutton = false;
    if ($settings['Platform'] == "Raspberry Pi") {
        exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
        $rootDevice = $output[0];
        if ($rootDevice == 'mmcblk0p2') {
?>
            <tr><td colspan='2'><hr></td></tr>
            <tr><td>
            <input type='button' class='buttons' value='Grow Filesystem' onClick='growSDCardFS();'>
            </td>
            <td><b>Grow file system on SD card</b> - This will grow the file system on the SD card to use
            the entire size of the SD card.</td>
            </tr>
<?php
            $addnewfsbutton = true;
        }
	}
    if ($settings['Platform'] == "BeagleBone Black") {
        exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
        $rootDevice = $output[0];
        if ($rootDevice == 'mmcblk0p1' || $rootDevice == 'mmcblk0p2') {
            $addnewfsbutton = true;
?>
            <tr><td colspan='2'><hr></td></tr>
            <tr><td>
                    <input type='button' class='buttons' value='Grow Filesystem' onClick='growSDCardFS();'>
                </td>
                <td><b>Grow filsystem on SD card</b> - This will grow the filesystem on the SD card to use
                    the entire size of the SD card.</td>
            </tr>
<?php
    if (strpos($settings['SubPlatform'], 'PocketBeagle') === FALSE) {
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
        }
    }
    if ($addnewfsbutton) {
?>
        <tr><td colspan='2'><hr></td></tr>
        <tr><td>
        <input type='button' class='buttons' value='New Partition' onClick='newSDCardPartition();'>
        </td>
        <td><b>New partition on SD card</b> - This will create a new partition in the unused aread of the SD card.  The new partition can be selected as a storage location and formatted to BTRFS or ext4 after a reboot.</td>
        </tr>
<?php
    }
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
                    <td ><? PrintSettingTextSaved("MQTTHost", 1, 0, 64, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Broker Port:</td>
                    <td ><? PrintSettingTextSaved("MQTTPort", 1, 0, 32, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Prefix:</td>
                    <td ><? PrintSettingTextSaved("MQTTPrefix", 1, 0, 32, 32, "", ""); ?></td>
                </tr>
                <tr>
                    <td >MQTT Username:</td>
                    <td ><? PrintSettingTextSaved("MQTTUsername", 1, 0, 32, 32, "", ""); ?></td>
		</tr>
                <tr>
                    <td >MQTT Password:</td>
                    <td ><? PrintSettingPasswordSaved("MQTTPassword", 1, 0, 32, 32, "", ""); ?></td>
                </tr>
	    </table>
            MQTT events will be published to "$prefix/falcon/player/$hostname/" with playlist events being in the "playlist" subtopic. <br/>
FPP will respond to certain events:
<div class="fppTableWrapper">
<table width = "100%" border="0" cellpadding="1" cellspacing="1">
<tr><th>Topic</th><th>Action</th></tr>
<tr>
<td>$prefix/falcon/player/$hostname/playlist/name/set</td><td>Starts the plalist named in the payload</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/playlist/repeat/set</td><td>If payload is "1", will turn on repeat, otherwise it is turned off</td>
</tr>
<tr>
<td>$prefix/falcon/player/$hostname/playlist/sectionPosition/set</td><td>Payload contains an integer for the position in the playlist</td>
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
