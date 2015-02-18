<?php
require_once("common.php");
require_once('universeentry.php');

?>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

/////////////////////////////////////////////////////////////////////////////
// E1.31 support functions here
$(document).ready(function() {
	$('.default-value').each(function() {
		var default_value = this.value;
		$(this).focus(function() {
			if(this.value == default_value) {
				this.value = '';
				$(this).css('color', '#333');
			}
		});
		$(this).blur(function() {
			if(this.value == '') {
				$(this).css('color', '#999');
				this.value = default_value;
			}
		});
	});

	$('#txtUniverseCount').on('focus',function() {
		$(this).select();
	});

	$('#tblUniverses').on('mousedown', 'tr', function(event,ui){
		$('#tblUniverses tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblUniverses tr');
		UniverseSelected  = items.index(this);
	});

	$('#frmUniverses').submit(function(event) {
			 event.preventDefault();
			 var success = validateUniverseData();
			 if(success == true)
			 {
				 dataString = $("#frmUniverses").serializeArray();
				 $.ajax({
						type: "post",
						url: "fppxml.php",
						dataType:"text",
						data: dataString,
						success: function (response) {
								getUniverses();
								$.jGrowl("E1.31 Universes Saved");
								SetRestartFlag();
						}
				}).fail( function() {
					DialogError("Save E1.31 Universes", "Save Failed");
				});
				return false;
			 }
			 else
			 {
			   DialogError("Save E1.31 Universes", "Validation Failed");
			 }
	});
});

<?
function PopulateInterfaces()
{
	global $settings;

	$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v usb0")));
	$ifaceE131 = "";
	if (isset($settings['E131interface'])) {
		$ifaceE131 = $settings['E131interface'];
	}
	$found = 0;
	if ($ifaceE131 == "") {
		$ifaceE131 = "eth0";
	}
	foreach ($interfaces as $iface)
	{
		$iface = preg_replace("/:$/", "", $iface);
		$ifaceChecked = "";
		if ($iface == $ifaceE131) {
			$ifaceChecked = " selected";
			$found = 1;
		}
		echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
	}
	if (!$found && ($ifaceE131 != "")) {
		echo "<option value='" . $ifaceE131 . "' selected>" . $ifaceE131 . "</option>";
	}
}
?>

/////////////////////////////////////////////////////////////////////////////
// Falcon Pixelnet/DMX support functions here
$(function() {
	$('#tblOutputs').on('mousedown', 'tr', function(event,ui){
		$('#tblOutputs tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblOutputs tr');
		PixelnetDMXoutputSelected  = items.index(this);
	});
});

$(document).ready(function(){
	$('#frmPixelnetDMX').submit(function(event) {
		 event.preventDefault();
		 dataString = $("#frmPixelnetDMX").serializeArray();
		 $.ajax({
			type: "post",
			url: "fppxml.php",
			dataType:"text",
			data: dataString
		}).success(function() {
			getPixelnetDMXoutputs('TRUE');
			$.jGrowl("FPD Config Saved");
			SetRestartFlag();
		}).fail(function() {
			DialogError("Save FPD Config", "Save Failed");
		});
		return false;
	});
});

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// 'Other' Channel Outputs support functions here
/////////////////////////////////////////////////////////////////////////////
// Misc. Support functions
function DeviceSelect(deviceArray, currentValue) {
	var result = "Port: <select class='device'>";

	if (currentValue == "")
		result += "<option value=''>-- Port --</option>";

	var found = 0;
	for (var key in deviceArray) {
		result += "<option value='" + key + "'";
	
		if (currentValue == key) {
			result += " selected";
			found = 1;
		}

		result += ">" + deviceArray[key] + "</option>";
	}

	if ((currentValue != '') &&
		(found == 0)) {
		result += "<option value='" + currentValue + "'>" + currentValue + "</option>";
	}
	result += "</select>";

	return result;
}

/////////////////////////////////////////////////////////////////////////////
// nRF Support functions
function nRFSpeedSelect(speedArray, currentValue) {
	var result = "Speed: <select class='speed'>";

	if ( currentValue == "" )
		result += "<option value=''>- Speed -</option>";

	var found = 0;
	for (var key in speedArray) {
		result += "<option value='" + key + "'";

		if (currentValue == key) {
			result += " selected";
			found = 1;
		}

		result += ">" + speedArray[key] + "</option>";
	}

	if ((currentValue != '') &&
		(found == 0)) {
		result += "<option value='" + currentValue + "'>" + currentValue + "</option>";
	}
	result += "</select> ";

	return result;
}

/////////////////////////////////////////////////////////////////////////////
// USB Dongles /dev/ttyUSB*
var USBDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if (preg_match("/^ttyUSB[0-9]+/", $fileName)) {
			echo "USBDevices['$fileName'] = '$fileName';\n";
		}
	}
?>

function USBDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(USBDevices, item[1]);
		}
	}

	return result;
}

function GetUSBOutputConfig(cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	return "device=" + value;
}

function NewUSBConfig() {
	var result = "";
	result += DeviceSelect(USBDevices, "");
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// RGBMatrix Output
function RGBMatrixLayoutSelect(currentValue) {
	var result = "";
	var options = "1x1,2x1,3x1,4x1,1x2,2x2,1x3,1x4".split(",");
	options = "1x1,2x1,3x1,4x1".split(",");

	result += " Layout (WxH):<select class='layout' onChange='RGBMatrixLayoutChanged(this);'>";

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];

		result += "<option value='" + opt + "'";
		if (currentValue == opt)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>";

	return result;
}

function RGBMatrixLayoutChanged(item) {
	var value = $(item).val();

	var size = value.split("x");

	var panels = parseInt(size[0]) * parseInt(size[1]);
	var channels = panels * 512 * 3;

	$(item).parent().parent().find("input.count").val(channels);
}

function NewRGBMatrixConfig() {
	var result = "";
	result += RGBMatrixLayoutSelect("");
	return result;
}

function RGBMatrixDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "layout") {
			result += RGBMatrixLayoutSelect(item[1]);
		}
	}

	return result;
}

function GetRGBMatrixOutputConfig(cell) {
	$cell = $(cell);

	var layout = $cell.find("select.layout").val();

	if (layout == "")
		return "";

	return "layout=" + layout;
}

/////////////////////////////////////////////////////////////////////////////
// LEDTriks/Triks-C Output
function TriksLayoutSelect(currentValue) {
	var result = "";
	var options = "1x1,2x1,3x1,4x1,1x2,2x2,1x3,1x4".split(",");

	result += " Layout (WxH):<select class='layout' onChange='TriksLayoutChanged(this);'>";

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];

		result += "<option value='" + opt + "'";
		if (currentValue == opt)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>";

	return result;
}

function TriksLayoutChanged(item) {
	var value = $(item).val();

	var size = value.split("x");

	var panels = parseInt(size[0]) * parseInt(size[1]);
	var channels = panels * 768 * 3;

	$(item).parent().parent().find("input.count").val(channels);
}

function NewTriksConfig() {
	var result = "";
	result += DeviceSelect(USBDevices, "");
	result += TriksLayoutSelect("");
	return result;
}

function TriksDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(USBDevices, item[1]);
		} else if (item[0] == "layout") {
			result += TriksLayoutSelect(item[1]);
		}
	}

	return result;
}

function GetTriksOutputConfig(cell) {
	$cell = $(cell);
	var device = $cell.find("select.device").val();

	if (device == "")
		return "";

	var layout = $cell.find("select.layout").val();

	if (layout == "")
		return "";

	return "device=" + device + ";layout=" + layout;
}

/////////////////////////////////////////////////////////////////////////////
// GPIO Pin direct high/low output

function GPIOGPIOSelect(currentValue) {
	var result = "";
<?
	if (isset($settings['PiFaceDetected']) && ($settings['PiFaceDetected'] == 1))
	{
?>
	var options = "4,5,6,12,13,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,200,201,202,203,204,205,206,207".split(",");
<?
	}
	else
	{
?>
	var options = "4,5,6,12,13,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31".split(",");
<?
	}
?>

	result += " BCM GPIO Output: <select class='gpio'>";

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];

		result += "<option value='" + opt + "'";
		if (currentValue == opt)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>";

	return result;
}

function NewGPIOConfig() {
	var result = "";
	result += GPIOGPIOSelect("");
	return result;
}

function GPIODeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "gpio") {
			result += GPIOGPIOSelect(item[1]);
		}
	}

	return result;
}

function GetGPIOOutputConfig(cell) {
	$cell = $(cell);
	var gpio = $cell.find("select.gpio").val();

	if (gpio == "")
		return "";

	return "gpio=" + gpio;
}

/////////////////////////////////////////////////////////////////////////////
// GPIO-attached 74HC595 Shift Register Output

function GPIO595GPIOSelect(currentValue) {
	var result = "";
	var options = "17-18-27,22-23-24".split(",");

	result += " BCM GPIO Outputs: <select class='gpio'>";

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];

		result += "<option value='" + opt + "'";
		if (currentValue == opt)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>";

	return result;
}

function NewGPIO595Config() {
	var result = "";
	result += GPIO595GPIOSelect("");
	return result;
}

function GPIO595DeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "gpio") {
			result += GPIO595GPIOSelect(item[1]);
		}
	}

	return result;
}

function GetGPIO595OutputConfig(cell) {
	$cell = $(cell);
	var gpio = $cell.find("select.gpio").val();

	if (gpio == "")
		return "";

	return "gpio=" + gpio;
}

/////////////////////////////////////////////////////////////////////////////
// Serial Devices used by Renard and LOR
var SerialDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^ttyS[0-9]+/", $fileName)) ||
			(preg_match("/^ttyACM[0-9]+/", $fileName)) ||
			(preg_match("/^ttyUSB[0-9]+/", $fileName))) {
			echo "SerialDevices['$fileName'] = '$fileName';\n";
		}
	}
?>
/////////////////////////////////////////////////////////////////////////////
// Renard Serial Outputs

var RenardSpeeds = new Array();
RenardSpeeds[ "19200"] =  "19200";
RenardSpeeds[ "38400"] =  "38400";
RenardSpeeds[ "57600"] =  "57600";
RenardSpeeds["115200"] = "115200";
RenardSpeeds["230400"] = "230400";
RenardSpeeds["460800"] = "460800";
RenardSpeeds["921600"] = "921600";

function RenardSpeedSelect(currentValue) {
	var result = "Speed: <select class='renardspeed'>";

	for (var key in RenardSpeeds) {
		result += "<option value='" + key + "'";
	
		if (currentValue == key) {
			result += " selected";
		}

		// Make 57.6k the default
		if ((currentValue == "") && (key == "57600")) {
			result += " selected";
		}

		result += ">" + RenardSpeeds[key] + "</option>";
	}

	result += "</select>";

	return result;
}

var RenardParms = new Array();
RenardParms["8N1"] = "8N1";
RenardParms["8N2"] = "8N2";

function RenardParmSelect(currentValue) {
	var result = " <select class='renardparm'>";

	for (var key in RenardParms) {
		result += "<option value='" + key + "'";

		if (currentValue == key) {
			result += " selected";
		}

		// Make 8N1 the default
		if ((currentValue == "") && (key == "8N1")) {
			result += " selected";
		}

		result += ">" + RenardParms[key] + "</option>";
	}

	result += "</select>";

	return result;
}

function RenardOutputConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(SerialDevices, item[1]) + "&nbsp;&nbsp;";
		} else if (item[0] == "renardspeed") {
			result += RenardSpeedSelect(item[1]);
		} else if (item[0] == "renardparm") {
			result += RenardParmSelect(item[1]);
		}
	}

	return result;
}

function GetRenardOutputConfig(cell) {
	$cell = $(cell);
	var result = "";
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result += "device=" + value + ";";

	value = $cell.find("select.renardspeed").val();

	if (value == "")
		return "";

	result += "renardspeed=" + value + ";";

	value = $cell.find("select.renardparm").val();

	if (value == "")
		return "";

	result += "renardparm=" + value;

	return result;
}

function NewRenardConfig() {
	var result = "";
	result += DeviceSelect(SerialDevices, "") + "&nbsp;&nbsp;";
	result += RenardSpeedSelect("");
	result += RenardParmSelect("");
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// LOR Serial Outputs

var LORSpeeds = new Array();
LORSpeeds[  "9600"] =   "9600";
LORSpeeds[ "19200"] =  "19200";
LORSpeeds[ "38400"] =  "38400";
LORSpeeds[ "57600"] =  "57600";
LORSpeeds["115200"] = "115200";

function LORSpeedSelect(currentValue) {
	var result = "Speed: <select class='speed'>";

	for (var key in LORSpeeds) {
		result += "<option value='" + key + "'";

		if (currentValue == key) {
			result += " selected";
		}

		// Make 19.2k the default
		if ((currentValue == "") && (key == "19200")) {
			result += " selected";
		}

		result += ">" + LORSpeeds[key] + "</option>";
	}

	result += "</select>";

	return result;
}

function LOROutputConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(SerialDevices, item[1]) + "&nbsp;&nbsp;";
		} else if (item[0] == "speed") {
			result += LORSpeedSelect(item[1]);
		}
	}

	return result;
}

function GetLOROutputConfig(cell) {
	$cell = $(cell);
	var result = "";
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result += "device=" + value + ";";

	value = $cell.find("select.speed").val();

	if (value == "")
		return "";

	result += "speed=" + value;

	return result;
}

function NewLORConfig() {
	var result = "";
	result += DeviceSelect(SerialDevices, "") + "&nbsp;&nbsp;";
	result += LORSpeedSelect("");
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// SPI Devices (/dev/spidev*
var SPIDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if (preg_match("/^spidev[0-9]/", $fileName)) {
			echo "SPIDevices['$fileName'] = '$fileName';\n";
		}
	}
?>

function SPIDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(SPIDevices, item[1]);
		}
	}

	return result;
}

var nRFSpeeds = ["250K", "1M", "2M"];

function SPInRFDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "speed") {
			result += nRFSpeedSelect(nRFSpeeds, item[1]);
		}
		else if (item[0] == "channel") {
			result += "Channel: <input class='channel' type='text' size=4 maxlength=4 value='";
			result += item[1] + "'>";
		}
	}

	return result;
}

function GetSPIOutputConfig(cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	return "device=" + value;
}

function GetnRFSpeedConfig(cell) {
	$cell = $(cell);
	var value = $cell.find("select.speed").val();

	if (value == "")
		return "";

	return "speed=" + value;
}

function NewSPIConfig() {
	var result = "";
	result += DeviceSelect(SPIDevices, "");
	return result;
}

function NewnRFSPIConfig() {
	var result = "";
	result += nRFSpeedSelect(nRFSpeeds, "");
	result += "Channel: <input class='channel' type='text' size=4 maxlength=4 value=''>";
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// 'Other' Channel Outputs misc. functions
function PopulateChannelOutputTable(data) {
	$('#tblOtherOutputs tbody').html("");

	for (var i = 0; i < data.Outputs.length; i++) {
		var output = data.Outputs[i].split(",");
		var type = output[1];

		var newRow = "<tr class='rowUniverseDetails'><td>" + (i + 1) + "</td>" +
				"<td><input class='act' type=checkbox";

		if (output[0] == "1")
			newRow += " checked";

		var countDisabled = "";

		if ((type == "Triks-C") || (type == 'GPIO') || (type == 'RGBMatrix'))
			countDisabled = " disabled=''";

		newRow += "></td>" +
				"<td>" + type + "</td>" +
				"<td><input class='start' type=text size=6 maxlength=6 value='" + output[2] + "'></td>" +
				"<td><input class='count' type=text size=4 maxlength=4 value='" + output[3] + "'" + countDisabled + "></td>" +
				"<td>";

		if ((type == "DMX-Pro") ||
			(type == "DMX-Open") ||
			(type == "Pixelnet-Lynx") ||
			(type == "Pixelnet-Open")) {
			newRow += USBDeviceConfig(output[4]);
		} else if (type == "Renard") {
			newRow += RenardOutputConfig(output[4]);
		} else if (type == "LOR") {
			newRow += LOROutputConfig(output[4]);
		} else if (type == "RGBMatrix") {
			newRow += RGBMatrixDeviceConfig(output[4]);
		} else if (type == "SPI-WS2801") {
			newRow += SPIDeviceConfig(output[4]);
		} else if (type == "SPI-nRF24L01") {
			newRow += SPInRFDeviceConfig(output[4]);
		} else if (type == "Triks-C") {
			newRow += TriksDeviceConfig(output[4]);
		} else if (type == "GPIO") {
			newRow += GPIODeviceConfig(output[4]);
		} else if (type == "GPIO-595") {
			newRow += GPIO595DeviceConfig(output[4]);
		}

		newRow += "</td>" +
				"</tr>";

		$('#tblOtherOutputs tbody').append(newRow);
	}
}

function GetChannelOutputs() {
	$.getJSON("fppjson.php?command=getChannelOutputs", function(data) {
		PopulateChannelOutputTable(data);
	});
}

function SetChannelOutputs() {
	var postData = {};
	var dataError = 0;
	var rowNumber = 1;
	var nRF = false;

	postData.Outputs = new Array();

	$('#tblOtherOutputs tbody tr').each(function() {
		$this = $(this);

		var output = "";

		// Enabled
		if ($this.find("td:nth-child(2) input.act").is(":checked")) {
			output += "1,";
		} else {
			output += "0,";
		}

		// Type
		var type = $this.find("td:nth-child(3)").html();

		// User has not selected a type yet
		if (type.indexOf("<select") >= 0) {
			DialogError("Save Channel Outputs",
				"Output type must be selected on row " + rowNumber);
			dataError = 1;
			return;
		}

		output += type + ",";

		// Get output specific options
		var config = "";
		var maxChannels = 510;
		if ((type == "DMX-Pro") ||
			(type == "DMX-Open") ||
			(type == "Pixelnet-Lynx") ||
			(type == "Pixelnet-Open")) {
			config += GetUSBOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 512;
			if (type.substring(0,8) == "Pixelnet")
				maxChannels = 4096;
		} else if (type == "Renard") {
			config += GetRenardOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 1528;
		} else if (type == "LOR") {
			config += GetLOROutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 3840;
		} else if (type == "RGBMatrix") {
			config += GetRGBMatrixOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 6144;
		} else if (type == "SPI-WS2801") {
			config += GetSPIOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 1530;
		} else if (type == "SPI-nRF24L01") {
			if (nRF)
			{
				dataError = 1;
				DialogError("Save Channel Outputs", "You already have an nRF Interface, only 1 currently allowed");
				return;
			}
			nRF = true;
			config += GetnRFSpeedConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid nRF Speed");
				return;
			}
			var channel = $this.find("td:nth-child(6)").find("input.channel").val();
			if (!channel && (channel < 0 || channel > 125) ) {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Channel '" + channel + "' on row " + rowNumber);
				return;
			}
			config = config+";channel="+channel;
			maxChannels = 512;
		} else if (type == "Triks-C") {
			config += GetTriksOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 9216;
		} else if (type == "GPIO") {
			config += GetGPIOOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 1;
		} else if (type == "GPIO-595") {
			config += GetGPIO595OutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 128;
		}

		// Channel Count
		var channelCount = $this.find("td:nth-child(5) input").val();
		if ((channelCount == "") || isNaN(channelCount) || (parseInt(channelCount) > maxChannels)) {
			DialogError("Save Channel Outputs",
				"Invalid Channel Count '" + channelCount + "' on row " + rowNumber);
			dataError = 1;
			return;
		}

		// Start Channel
		var startChannel = $this.find("td:nth-child(4) input").val();
		if ((startChannel == "") || isNaN(startChannel)) {
			DialogError("Save Channel Outputs",
				"Invalid Start Channel '" + startChannel + "' on row " + rowNumber);
			dataError = 1;
			return;
		}

		var endChannel = parseInt(startChannel) + parseInt(channelCount) - 1;
		if (endChannel > 131072) {
			DialogError("Save Channel Outputs",
				"Start Channel '" + startChannel + "' plus Channel Count '" + channelCount + "' exceeds 131072 on row " + rowNumber);
			dataError = 1;
			return;
		}

		output += startChannel + ",";
		output += channelCount + ",";
		output += config;

		postData.Outputs.push(output);

		rowNumber++;
	});

	if (dataError)
		return;

	postData = "command=setChannelOutputs&data=" + JSON.stringify(postData);

	$.post("fppjson.php", postData).success(function(data) {
		PopulateChannelOutputTable(data);
		$.jGrowl("Channel Output Configuration Saved");
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Channel Outputs", "Save Failed");
	});
}

function AddOtherTypeOptions(row, type) {
	var config = "";

	row.find("td input.start").val("1");
	row.find("td input.start").show();
	row.find("td input.count").show();

	if ((type == "DMX-Pro") ||
		(type == "DMX-Open")) {
		config += NewUSBConfig();
		row.find("td input.count").val("512");
	} else if ((type == "Pixelnet-Lynx") ||
			   (type == "Pixelnet-Open")) {
		config += NewUSBConfig();
		row.find("td input.count").val("4096");
	} else if (type == "Renard") {
		config += NewRenardConfig();
		row.find("td input.count").val("286");
	} else if (type == "LOR") {
		config += NewLORConfig();
		row.find("td input.count").val("16");
		row.find("td input.speed").val("19200");
	} else if (type == "RGBMatrix") {
		config += NewRGBMatrixConfig();
		row.find("td input.count").val("1536");
		row.find("td input.count").prop('disabled', true);
	} else if (type == "SPI-WS2801") {
		config += NewSPIConfig();
		row.find("td input.count").val("1530");
	} else if (type == "SPI-nRF24L01") {
		config += NewnRFSPIConfig();
		row.find("td input.count").val("512");
	} else if (type == "Triks-C") {
		config += NewTriksConfig();
		row.find("td input.count").val("2304");
		row.find("td input.count").prop('disabled', true);
	} else if (type == "GPIO") {
		config += NewGPIOConfig();
		row.find("td input.count").val("1");
		row.find("td input.count").prop('disabled', true);
	} else if (type == "GPIO-595") {
		config += NewGPIO595Config();
		row.find("td input.count").val("8");
	}

	row.find("td:nth-child(6)").html(config);
}

function OtherTypeSelected(selectbox) {
	$row = $(selectbox.parentNode.parentNode);

	var type = $(selectbox).val();

	if ((Object.keys(USBDevices).length == 0) &&
			((type == 'DMX-Pro') ||
			 (type == 'DMX-Open') ||
			 (type == 'Pixelnet-Lynx') ||
			 (type == 'Pixelnet-Open') ||
			 (type == 'Triks-C')))
	{
		DialogError("Add Output", "No available serial devices detected.  Do you have a USB Serial Dongle attached?");
		$row.remove();
		return;
	}

	if ((Object.keys(SerialDevices).length == 0) &&
			((type == 'Renard') || (type == 'LOR')))
	{
		DialogError("Add Output", "No available serial devices detected.");
		$row.remove();
		return;
	}

	if ((Object.keys(SPIDevices).length == 0) &&
			(type == 'SPI-WS2801'))
	{
		DialogError("Add Output", "No available SPI devices detected.");
		$row.remove();
		return;
	}

	if ((type == 'Triks-C') || (type == 'GPIO') || (type == 'RGBMatrix'))
	{
		$row.find('input.count').prop('disabled', true);
	}

	$row.find("td:nth-child(3)").html(type);

	AddOtherTypeOptions($row, type);
}

function AddOtherOutput() {
	if ((Object.keys(USBDevices).length == 0) &&
		(Object.keys(SerialDevices).length == 0) &&
		(Object.keys(SPIDevices).length == 0)) {
		DialogError("Add Output", "No available devices found for new outputs");
		return;
	}

	var currentRows = $("#tblOtherOutputs > tbody > tr").length;

	var newRow = 
		"<tr class='rowUniverseDetails'><td>" + (currentRows + 1) + "</td>" +
			"<td><input class='act' type=checkbox></td>" +
			"<td><select class='type' onChange='OtherTypeSelected(this);'>" +
				"<option value=''>Select a type</option>" +
				"<option value='DMX-Pro'>DMX-Pro</option>" +
				"<option value='DMX-Open'>DMX-Open</option>" +
				"<option value='Pixelnet-Lynx'>Pixelnet-Lynx</option>" +
				"<option value='Pixelnet-Open'>Pixelnet-Open</option>" +
				"<option value='LOR'>LOR</option>" +
				"<option value='Renard'>Renard</option>" +
				"<option value='RGBMatrix'>RGBMatrix</option>" +
				"<option value='SPI-WS2801'>SPI-WS2801</option>" +
				"<option value='SPI-nRF24L01'>SPI-nRF24L01</option>" +
				"<option value='Triks-C'>Triks-C</option>" +
				"<option value='GPIO'>GPIO</option>" +
				"<option value='GPIO-595'>GPIO-595</option>" +
			"</select></td>" +
			"<td><input class='start' type='text' size=6 maxlength=6 value='' style='display: none;'></td>" +
			"<td><input class='count' type='text' size=4 maxlength=4 value='' style='display: none;'></td>" +
			"<td><input class='channel' type='text' size=4 maxlength=4 value='' style='display: none;'></td>" +
			"</tr>";

	$('#tblOtherOutputs tbody').append(newRow);
}

var otherTableInfo = {
	tableName: "tblOtherOutputs",
	selected:  -1,
	enableButtons: [ "btnDeleteOther" ],
	disableButtons: []
	};

function RenumberColumns(tableName) {
	var id = 1;
	$('#' + tableName + ' tbody tr').each(function() {
		$this = $(this);
		$this.find("td:first").html(id);
		id++;
	});
}

function DeleteOtherOutput() {
	if (otherTableInfo.selected >= 0) {
		$('#tblOtherOutputs tbody tr:nth-child(' + (otherTableInfo.selected+1) + ')').remove();
		otherTableInfo.selected = -1;
		SetButtonState("#btnDeleteOther", "disable");
		RenumberColumns("tblOtherOutputs");
	}
}

$(document).ready(function(){
	// E1.31 initialization
	InitializeUniverses();
	getUniverses('TRUE');

	// FPD initialization
	getPixelnetDMXoutputs('TRUE');

	// 'Other' Channel Outputs initialization
	SetupSelectableTableRow(otherTableInfo);
	GetChannelOutputs();

	// Init tabs
    $("#tabs").tabs({cache: true, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });
});

</script>
<!-- FIXME, move this to CSS to standardize the UI -->
<style>
.tblheader{
    background-color:#CCC;
    text-align:center;
    }
tr.rowUniverseDetails
{
    border:thin solid;
    border-color:#CCC;
}

tr.rowUniverseDetails td
{
    padding:1px 5px;
}

.channelOutputTable
{
    border:thin;
    border-color:#333;
    border-collapse: collapse;
}

</style>

<title><? echo $pageTitle; ?></title>
</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

<div id='channelOutputManager'>
		<div class='title'>Channel Outputs</div>
		<div id="tabs">
			<ul>
				<li><a href="#tab-e131">E1.31</a></li>
				<li><a href="#tab-fpd">Falcon Pixelnet/DMX</a></li>
				<li><a href="#tab-other">Other</a></li>
			</ul>
			<div id='tab-e131'>
				<div id='divE131'>
					<fieldset class="fs">
						<legend> E1.31 </legend>
						<div id='divE131Data'>

<!-- --------------------------------------------------------------------- -->

  <div style="overflow: hidden; padding: 10px;">
	<b>Enable E1.31 Output:</b> <? PrintSettingCheckbox("E1.31 Output", "E131Enabled", 1, 0, "1", "0"); ?><br><br>
	E1.31 Interface: <select id="selE131interfaces" onChange="SetE131interface();"><? PopulateInterfaces(); ?></select>
	<br><br>

    <div>
      <form>
        Universe Count: <input id="txtUniverseCount" class="default-value" type="text" value="Enter Universe Count" size="3" maxlength="3" /><input id="btnUniverseCount" onclick="SetUniverseCount();" type="button"  class="buttons" value="Set" />
      </form>
    </div>
    <form id="frmUniverses">
    <input name="command" type="hidden" value="saveUniverses" />
    <table>
    	<tr>
      	<td width = "70 px"><input id="btnSaveUniverses" class="buttons" type="submit" value = "Save" /></td>
      	<td width = "70 px"><input id="btnCloneUniverses" class="buttons" type="button" value = "Clone" onClick="CloneUniverse();" /></td>
      	<td width = "40 px">&nbsp;</td>
      	<td width = "70 px"><input id="btnDeleteUniverses" class="buttons" type="button" value = "Delete" onClick="DeleteUniverse();" /></td>
      </tr>
    </table>
    
		<table id="tblUniverses" class='channelOutputTable'>
    </table>
		</form>
	</div>

<!-- --------------------------------------------------------------------- -->

						</div>
					</fieldset>
				</div>
			</div>
			<div id='tab-fpd'>
				<div id='divFPD'>
					<fieldset class="fs">
						<legend> Falcon Pixelnet/DMX </legend>
						<div id='divFPDData'>

<!-- --------------------------------------------------------------------- -->

							<div style="overflow: hidden; padding: 10px;">
								<b>Enable FPD Output:</b> <? PrintSettingCheckbox("FPD Output", "FPDEnabled", 1, 0, "1", "0"); ?><br><br>
								<form id="frmPixelnetDMX">
									<input name="command" id="command"  type="hidden" value="saveHardwareConfig" />
                  <input name='model' id="model" type='hidden' value='FPDv1' />
                  <input name='firmware' id="firmware" type='hidden' value='1.10' />

									<table>
										<tr>
											<td width = "70 px"><input id="btnSaveOutputs" class="buttons" type="submit" value = "Save" /></td>
											<td width = "70 px"><input id="btnCloneOutputs" class="buttons" type="button" value = "Clone" onClick="ClonePixelnetDMXoutput();" /></td>
											<td width = "40 px">&nbsp;</td>
										</tr>
									</table>
									<table id="tblOutputs" class='channelOutputTable'>
									</table>
								</form>
							</div>

<!-- --------------------------------------------------------------------- -->

				</div>
			</fieldset>
		</div>
	</div>
	<div id='tab-other'>
		<div id='divOther'>
			<fieldset class="fs">
				<legend> Other Outputs </legend>
				<div id='divOtherData'>

<!-- --------------------------------------------------------------------- -->

<div style="overflow: hidden; padding: 10px;">
<form id="frmOtherOutputs">
<input name="command" type="hidden" value="saveOtherOutputs" />
<table>
<tr>
<td width = "70 px"><input id="btnSaveOther" class="buttons" type="button" value = "Save" onClick='SetChannelOutputs();' /></td>
<td width = "70 px"><input id="btnAddOther" class="buttons" type="button" value = "Add" onClick="AddOtherOutput();"/></td>
<td width = "40 px">&nbsp;</td>
<td width = "70 px"><input id="btnDeleteOther" class="disableButtons" type="button" value = "Delete" onClick="DeleteOtherOutput();"></td>
</tr>
</table>
<table id="tblOtherOutputs" class='channelOutputTable'>
<thead>
	<tr class='tblheader'><td>#</td><td>Act</td><td>Type</td><td>Start Ch.</td><td>Ch. Cnt</td><td>Output Config</td></tr>
</thead>
<tbody>
</tbody>
</table>
</form>
</div>

<!-- --------------------------------------------------------------------- -->

				</div>
			</fieldset>
		</div>
	</div>
</div>
</div>

	</div>

	<?php	include 'common/footer.inc'; ?>
</body>
</html>
