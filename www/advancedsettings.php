<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="global" class="settings">
	<fieldset>
	<legend>FPP Advanced Settings</legend>
	<table table width = "100%">
		<tr><td valign='top'><? PrintSettingSelect("E1.31 Bridging Transmit Interval", "E131BridgingInterval", 1, 0,"50", Array('10ms' => '10', '25ms' => '25', '40ms' => '40', '50ms' => '50', '100ms' => '100')); ?></td>
			<td valign='top'><b>E1.31 Bridge Mode Transmit Interval</b> - The
				default Transmit Interval in E1.31 Bridge Mode is 50ms.  This
				setting allows changing this to match the rate the player is
				outputting. <font color='#ff0000'><b>WARNING</b></font> - Some
				output devices such as the FPD do not support rates other than 50ms.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingCheckbox("E1.31 to E1.31 Bridging", "E131Bridging", 1, 0, "1", "0"); ?></td>
			<td valign='top'><b>Enable E1.31 to E1.31 Bridging</b> - 
				<font color='#ff0000'><b>WARNING</b></font> -
				E1.31 to E1.31 bridging over wireless is not recommended for
				live show use.  It is provided here as an option to allow
				testing remote wireless-attached FPP systems.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingText("E131Priority", 1, 0, 3, 3, "", "0"); ?><br>
				<? PrintSettingSave("E1.31 Priority", "E131Priority", 1, 0); ?></td>
			<td valign='top'><b>E1.31 Priority</b> - The E1.31 priority allows
				multiple players to send data to the same device at the same
				time.  The packets with the highest priority are used.  This
				functionality is not supported by all controllers, so changing
				it may not have any effect with your controllers.
				<font color='#ff0000'><b>WARNING</b></font> - Sending multiple
				packets for the same universe to the same controller will
				cause flicker if the controller does not support priority or
				if both players send packets with the same priority.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingText("mediaOffset", 1, 0, 5, 5); ?> ms<br>
				<? PrintSettingSave("Media Offset", "mediaOffset", 1, 0); ?></td>
			<td valign='top'><b>Media/Sequence Offset</b> - The media offset value
				allows adjusting the synchronization of the media and sequences being
				played.  The value is specified in milliseconds.  A positive value
				moves the media ahead, a negative value moves the media back.
				Changing this value requires a FPPD restart.
				<font color='#ff0000'><b>WARNING</b></font> - This offset applies
				to all media files played.  If your media files require different
				offsets per file then you will have to edit the audio files or
				sequences to bring them into sync.</td>
		</tr>
	</table>
	</fieldset>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
