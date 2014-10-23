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
		<tr><td valign='top'><? PrintSettingSelect("E1.31 Bridging Transmit Interval", "E131BridgingInterval", "50", Array('25ms' => '25', '40ms' => '40', '50ms' => '50', '100ms' => '100')); ?></td>
			<td valign='top'><b>E1.31 Bridge Mode Transmit Interval</b> - The
				default Transmit Interval in E1.31 Bridge Mode is 50ms.  This
				setting allows changing this to match the rate the player is
				outputting. <font color='#ff0000'><b>WARNING</b></font> - Some
				output devices such as the FPD do not support rates other than 50ms.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingCheckbox("E1.31 to E1.31 Bridging", "E131Bridging", "1", "0"); ?></td>
			<td valign='top'><b>Enable E1.31 to E1.31 Bridging</b> - 
				<font color='#ff0000'><b>WARNING</b></font> -
				E1.31 to E1.31 bridging over wireless is not recommended for
				live show use.  It is provided here as an option to allow
				testing remote wireless-attached Pi's.</td>
		</tr>
	</table>
	</fieldset>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
