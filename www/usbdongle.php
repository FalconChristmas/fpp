<?php
require_once("common.php");

echo "<!--\n";
$usbDonglePort = ReadSettingFromFile("USBDonglePort");
if (($usbDonglePort == false) || ($usbDonglePort == ""))
	$usbDonglePort = "DISABLED";
$usbDongleType = ReadSettingFromFile("USBDongleType");
if ($usbDongleType == false)
	$usbDongleType = "DMXPro";
$usbDongleBaud = ReadSettingFromFile("USBDongleBaud");
if ($usbDongleBaud == false)
	$usbDongleBaud = 57600; //We'll assume 56k is the most common
echo "-->\n";

function PrintUSBSerialPortOptions()
{
	global $usbDonglePort;

	echo "<option value='DISABLED' ";
	if ($usbDonglePort == "DISABLED")
		echo "SELECTED";
	echo ">-- DISABLED --</option>\n";

	foreach(scandir("/dev/") as $devFile)
	{
		if (preg_match('/^ttyUSB.*/', $devFile))
		{
			echo "<option value='$devFile' ";
			if ($devFile == $usbDonglePort)
				echo "SELECTED";
			echo ">$devFile</option>\n";
		}
	}
}

?>
<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>


<div id="time" class="settings">
<fieldset>
<legend>USB Dongle Setup</legend>
<table>
<tr><td>USB Dongle:</td><td><select id='USBDonglePort'>
<?
	PrintUSBSerialPortOptions();
?>
</select></td></tr>
<tr><td>Dongle Type*:</td><td>
	<select id='USBDongleType'>
		<option value='DMXOpen' <? if ($usbDongleType == "DMXOpen") echo "SELECTED"; ?>>DMXOpen</option>
		<option value='DMXPro' <? if ($usbDongleType == "DMXPro") echo "SELECTED"; ?>>DMXPro</option>
		<option value='Pixelnet' <? if ($usbDongleType == "Pixelnet") echo "SELECTED"; ?>>Pixelnet</option>
		<option value='Renard' <? if ($usbDongleType == "Renard") echo "SELECTED"; ?>>Renard</option>
	</select>
</td></tr>
<tr id="RenardBaud" style="display: <?php echo ($usbDongleType == "Renard" ? "table-row" : "none"); ?>"><td>Baud Rate:</td><td>
	<select id='USBDongleBaud'>
		<option value='19200' <? if ($usbDongleBaud == "19200") echo "SELECTED"; ?>>19200</option>
		<option value='38400' <? if ($usbDongleBaud == "38400") echo "SELECTED"; ?>>38400</option>
		<option value='57600' <? if ($usbDongleBaud == "57600") echo "SELECTED"; ?>>57600</option>
		<option value='76800' <? if ($usbDongleBaud == "76800") echo "SELECTED"; ?>>76800</option>
		<option value='115200' <? if ($usbDongleBaud == "115200") echo "SELECTED"; ?>>115200</option>
		<option value='230400' <? if ($usbDongleBaud == "230400") echo "SELECTED"; ?>>230400</option>
		<option value='460800' <? if ($usbDongleBaud == "460800") echo "SELECTED"; ?>>460800</option>
	</select>
</td></tr>
</table>
<br>
<input type=submit value='Submit' onClick='SaveUSBDongleSettings();'>

<hr>
<font size=-1>
* The DMX-Pro dongle support should be compatible with Entec Pro, Lynx DMX,
DIYC RPM, DMXking.com, and DIYblinky.com dongles using a universe size of
up to 512 channels.
The DMX-Open dongle support should be compatible with FTDI-based USB to
serial converters including Entec Open, LOR, and D-Light.
The Pixelnet-Lynx dongle support is compatible with the Lynx Pixelnet USB
dongle using up to 4096 channels.  Currently only one dongle is
supported at a time and FPP always outputs the first universe
in the channel data.</font>
</fieldset>
</div>

<script type="text/javascript">
$('#USBDongleType').on("change", function(){
	var type = $('#USBDongleType option:selected').text();
	if ( type == "Renard" )
		$('#RenardBaud').css("display", "table-row");
	else
		$('#RenardBaud').css("display", "none");
});
</script>

</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
