
<html>
<head>
<?php
require_once("common.php");
require_once('universeentry.php');
include 'common/menuHead.inc';
?>
<script language="Javascript">

var channelOutputs = [];
var channelOutputsLookup = [];

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

	$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v usb | grep -v SoftAp | grep -v 'can.'")));
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
// Serial Devices
var SerialDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^ttyS[0-9]+/", $fileName)) ||
			(preg_match("/^ttyACM[0-9]+/", $fileName)) ||
			(preg_match("/^ttyO[0-9]/", $fileName)) ||
			(preg_match("/^ttyS[0-9]/", $fileName)) ||
			(preg_match("/^ttyAMA[0-9]+/", $fileName)) ||
			(preg_match("/^ttyUSB[0-9]+/", $fileName))) {
			echo "SerialDevices['$fileName'] = '$fileName';\n";
		}
	}
?>

/////////////////////////////////////////////////////////////////////////////
function USBDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(SerialDevices, item[1]);
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
	result += DeviceSelect(SerialDevices, "");
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// LED Panel Layout support functions
var panelRowButton = 0;
function EditLEDPanelLayout(button) {
	panelRowButton = button;
	$row = $(button.parentNode.parentNode);

	var layout = $row.find("td select.layout").val();
	var panels = $row.find("td input.panels").val();
	var panelCount = parseInt($row.find("td select.panelCount").val());
	var panelOptions = $row.find("td input.panelOptions").val();

	var html = "";

	html += DrawPanelLayout(panelOptions, panelCount, panels);

	html += "<br>";
	html += "<input type='button' value='Save' onClick='SavePanelLayout();'> ";
	html += "<input type='button' value='Cancel' onClick='CancelPanelLayout();'>";

	$('#layoutText').html(html);
	$('#dialog-panelLayout').dialog({ height: 600, width: 800, title: "Panel Layout" });
	$('#dialog-panelLayout').dialog( "moveToTop" );
}

function GetPanelTable(layout) {
	var dimensions = layout.split("x");
	var width = parseInt(dimensions[0]);
	var height = parseInt(dimensions[1]);
	var table = "";

	for (y = 0; y < height; y++)
	{
		table += "<tr>";
		for (x = 0; x < width; x++)
		{
			table += "<td>";
			table += "(" + x + "," + y + ")";
			table += "</td>";
		}
		table += "</tr>";
	}

	return table;
}

function DrawPanelTable(layout) {
	var table = GetPanelTable(layout);
	$('#panelLayoutTable').html(table);
}

function PanelLayoutChanged() {
	DrawPanelTable($('#panelEditLayout').val());
}

function DrawPanelLayout(panelOptions, panelCount, panelsStr) {
	var options = panelOptions.split(';');
	var maxOutputs = parseInt(options[0]);
	var maxPanelsPerOutput = parseInt(options[1]);
	var result = "";
	result += "Outputs: " + maxOutputs + "<br>";
	result += "Max Panels Per Output: " + maxPanelsPerOutput + "<br>";
	result += "Panel Count: " + panelCount + "<br>";
	result += "Panels: " + panelsStr + "<br>";
	result += "<table border=1 cellpadding=3 cellspacing=1>";
	result += "<tr><th>Output #</th><th>Panel #</th><th>Orientation</th><th>X Offset</th><th>Y Offset</th></tr>";
	var panels = panelsStr.split("|");
	for (p = 0; p < panels.length; p++) {
		result += "<tr>";
		var parts = panels[p].split(":");
		for (i = 0; i < parts.length; i++) {
			result += "<td>" + parts[i] + "</td>";
		}
		result += "</tr>";
	}

	result += "</table>";

	return result;
}

function SavePanelLayout() {
	$row = $(panelRowButton.parentNode.parentNode);
	var panels = "";

	$row.find("td input.panels").val(panels);
	$('#dialog-panelLayout').dialog("destroy");
}

function CancelPanelLayout() {
	$('#dialog-panelLayout').dialog("destroy");
}

/////////////////////////////////////////////////////////////////////////////
// Virtual Matrix Output
function VirtualMatrixLayoutChanged(item) {
	var val = parseInt($(item).val());

	if ((val % 16) != 0)
	{
		alert("ERROR: Value must be divisible by 16");
	}

	var width = $(item).parent().parent().find("input.width").val();
	var height = $(item).parent().parent().find("input.height").val();
	var channels = width * height * 3;

	$(item).parent().parent().find("input.count").val(channels);
}

function VirtualMatrixColorOrderSelect(colorOrder) {
	var result = "";

	result += " Color Order: <select class='colorOrder'>";
	result += "<option value='RGB'";

	if (colorOrder == 'RGB')
		result += " selected";

	result += ">RGB</option><option value='BGR'";

	if (colorOrder != 'RGB')
		result += " selected";

	result += ">BGR</option></select>";

	return result;
}

function VirtualMatrixConfig(cfgStr) {
	var result = "";
	var items = cfgStr.split(";");
	var foundRGB = 0;

	for (var j = 0; j < items.length; j++)
	{
		var parts = items[j].split("=");
		var vals = parts[1].split("x");

		if (parts[0] == "layout")
		{
			result += " Width: <input type='text' size='3' maxlength='3' class='width' value='" + vals[0] + "' onChange='VirtualMatrixLayoutChanged(this);'>" +
				" Height: <input type='text' size='3' maxlength='3' class='height' value='" + vals[1] + "' onChange='VirtualMatrixLayoutChanged(this);'>";
		}
		else if (parts[0] == "colorOrder")
		{
			foundRGB = 1;
			result += VirtualMatrixColorOrderSelect(parts[1]);
		}
	}

	if (foundRGB == 0)
		result += VirtualMatrixColorOrderSelect("BGR");

	return result;
}

function NewVirtualMatrixConfig() {
	return VirtualMatrixConfig("layout=32x16;colorOrder=RGB");
}

function GetVirtualMatrixOutputConfig(cell) {
	$cell = $(cell);
	var width = $cell.find("input.width").val();

	if (width == "")
		return "";

	var height = $cell.find("input.height").val();

	if (height == "")
		return "";

	var colorOrder = $cell.find("select.colorOrder").val();

	if (colorOrder == "")
		return "";

	return "layout=" + width + "x" + height + ";colorOrder=" + colorOrder;
}

/////////////////////////////////////////////////////////////////////////////
// Generic Serial

var GenericSerialSpeeds = new Array();
GenericSerialSpeeds[  "9600"] =   "9600";
GenericSerialSpeeds[ "19200"] =  "19200";
GenericSerialSpeeds[ "38400"] =  "38400";
GenericSerialSpeeds[ "57600"] =  "57600";
GenericSerialSpeeds["115200"] = "115200";
GenericSerialSpeeds["230400"] = "230400";
GenericSerialSpeeds["460800"] = "460800";
GenericSerialSpeeds["921600"] = "921600";

function GenericSerialSpeedSelect(currentValue) {
	var result = "Speed: <select class='speed'>";

	for (var key in GenericSerialSpeeds) {
		result += "<option value='" + key + "'";

		if (currentValue == key) {
			result += " selected";
		}

		// Make 9600 the default
		if ((currentValue == "") && (key == "9600")) {
			result += " selected";
		}

		result += ">" + GenericSerialSpeeds[key] + "</option>";
	}

	result += "</select>";

	return result;
}

function GenericSerialConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(SerialDevices, item[1]);
		} else if (item[0] == "speed") {
			result += GenericSerialSpeedSelect(item[1]);
		} else if (item[0] == "header") {
			result += " Header: <input type='text' size=10 maxlength=20 class='serialheader' value='" + item[1] + "'>";
		} else if (item[0] == "footer") {
			result += " Footer: <input type='text' size=10 maxlength=20 class='serialfooter' value='" + item[1] + "'>";
		}
	}

	return result;
}

function NewGenericSerialConfig() {
	return GenericSerialConfig("device=ttyUSB0;speed=9600;header=;footer=");
}

function GetGenericSerialConfig(cell) {
	$cell = $(cell);
	var device = $cell.find("select.device").val();

	if (device == "")
		return "";

	var speed = $cell.find("select.speed").val();

	if (speed == "")
		return "";

	var header = $cell.find("input.serialheader").val();

	var footer = $cell.find("input.serialfooter").val();

	return "device=" + device + ";speed=" + speed + ";header=" + header + ";footer=" + footer;
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
	result += DeviceSelect(SerialDevices, "");
	result += TriksLayoutSelect("");
	return result;
}

function TriksDeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(SerialDevices, item[1]);
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
// USB Relay output
function USBRelayLayoutChanged(item) {
	var channels = parseInt($(item).val());

	$(item).parent().parent().find("input.count").val(channels);
}

function USBRelaySubTypeSelect(currentValue) {
	var result = "";
	var options = "Bit,ICStation".split(",");

	result += " Type:&nbsp;<select class='subType'>";

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];

		result += "<option value='" + opt + "'";
		if (currentValue == opt)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>  ";

	return result;
}

function USBRelayCountSelect(currentValue) {
	var result = "";
	var options = "2,4,8".split(",");

	result += " Count:&nbsp;<select class='relayCount' onChange='USBRelayLayoutChanged(this);'>";

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];

		result += "<option value='" + opt + "'";
		if (currentValue == opt)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>  ";

	return result;
}

function USBRelayConfig(cfgStr) {
	var result = "";
	var items = cfgStr.split(";");

	for (var j = 0; j < items.length; j++)
	{
		var item = items[j].split("=");

		if (item[0] == "device")
		{
			result += DeviceSelect(SerialDevices, item[1]) + "  ";
		}
		else if (item[0] == "subType")
		{
			result += USBRelaySubTypeSelect(item[1]);
		}
		else if (item[0] == "relayCount")
		{
			result += USBRelayCountSelect(item[1]);
		}
	}

	return result;
}

function NewUSBRelayConfig() {
	return USBRelayConfig("device=;subType=ICStation;relayCount=2");
}

function GetUSBRelayOutputConfig(cell) {
	$cell = $(cell);
	var result = "";
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result += "device=" + value + ";";

	value = $cell.find("select.subType").val();

	if (value == "")
		return "";

	result += "subType=" + value + ";";

	value = $cell.find("select.relayCount").val();

	if (value == "")
		return "";

	result += "relayCount=" + value;

	return result;
}

/////////////////////////////////////////////////////////////////////////////
// Raspberry Pi WS281x output
function RPIWS281XLayoutChanged(item) {
	var string1Pixels = parseInt($(item).parent().parent().find("input.string1Pixels").val());
	var string2Pixels = parseInt($(item).parent().parent().find("input.string2Pixels").val());

	if (string1Pixels > 1000) {
		DialogError("Invalid Pixel Count", "Invalid Pixel Count, max 1000");
		$(item).parent().parent().find("input.string1Pixels").val("1000");
		string1Pixels = 1000;
	}

	if (string2Pixels > 1000) {
		DialogError("Invalid Pixel Count", "Invalid Pixel Count, max 1000");
		$(item).parent().parent().find("input.string2Pixels").val("1000");
		string2Pixels = 1000;
	}

	var channels = (string1Pixels + string2Pixels) * 3;

	$(item).parent().parent().find("input.count").val(channels);
}

function RPIWS281XColorOrderSelect(id, colorOrder) {
	var options = ["RGB", "RBG", "GRB", "GBR", "BRG", "BGR"];
	var result = "";

	result += " Color Order: <select class='" + id + "'>";

    var i = 0;
	for (i = 0; i < options.length; i++)
	{
		result += "<option value='" + options[i] + "'";

		if (options[i] == colorOrder)
			result += " selected='selected'";

		result += ">" + options[i] + "</option>";
	}

	result += "</select>";

	return result;
}

function RPIWS281XConfig(cfgStr) {
	var result = "";
	var items = cfgStr.split(";");
	
	var data = {};
	
	for (var j = 0; j < items.length; j++)
	{
		var item = items[j].split("=");

   		data[item[0]] = item[1];
	}
	
	result += "String #1 Pixels: <input class='string1Pixels' size='4' maxlength='4' value='" + data["string1Pixels"] + "' onChange='RPIWS281XLayoutChanged(this);'> ";
	result += RPIWS281XColorOrderSelect("string1ColorOrder", data["string1ColorOrder"]) + " (GPIO 18)<br>";
	
	result += "String #2 Pixels: <input class='string2Pixels' size='4' maxlength='4' value='" + data["string2Pixels"] + "' onChange='RPIWS281XLayoutChanged(this);'> ";
	result += RPIWS281XColorOrderSelect("string2ColorOrder", data["string2ColorOrder"]) + " (GPIO 19)<br>";

	return result;
}

function NewRPIWS281XConfig() {
	return RPIWS281XConfig("string1Pixels=1;string1ColorOrder=RGB;string2Pixels=0;string2ColorOrder=RGB");
}

function GetRPIWS281XOutputConfig(cell) {
	$cell = $(cell);
	var result = "";
	var value = $cell.find("input.string1Pixels").val();

	if (parseInt(value) > 1000)
		return "";

	if (value == "")
		return "";

	result += "string1Pixels=" + value + ";";
	
	var colorOrder = $cell.find("select.string1ColorOrder").val();

	if (colorOrder == "")
		return "";
	
	result += "string1ColorOrder=" + colorOrder + ";";
	
	value = $cell.find("input.string2Pixels").val();

	if (parseInt(value) > 1000)
		return "";

	if (value == "")
		return "";

	result += "string2Pixels=" + value + ";";

	colorOrder = $cell.find("select.string2ColorOrder").val();

	if (colorOrder == "")
		return "";
	
	result += "string2ColorOrder=" + colorOrder;

	return result;
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
	result += " Invert: <input type=checkbox class='invert'>&nbsp;&nbsp;SoftPWM: <input type=checkbox class='softPWM'>";
	return result;
}

function GPIODeviceConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "gpio") {
			result += GPIOGPIOSelect(item[1]);
		} else if (item[0] == "invert") {
			result += " Invert: <input type=checkbox class='invert'";
			if (item[1] == "1")
				result += " checked='checked'";
			result += ">";
		} else if (item[0] == "softPWM") {
			result += " SoftPWM: <input type=checkbox class='softPWM'";
			if (item[1] == "1")
				result += " checked='checked'";
			result += ">";
		}
	}

	return result;
}

function GetGPIOOutputConfig(cell) {
	$cell = $(cell);
	var gpio = $cell.find("select.gpio").val();

	if (gpio == "")
		return "";

	var invert = 0;
	var softPWM = 0;

	if ($cell.find("input.invert").is(":checked"))
		invert = 1;

	if ($cell.find("input.softPWM").is(":checked"))
		softPWM = 1;

	return "gpio=" + gpio + ";invert=" + invert + ";softPWM=" + softPWM;
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
		} else if (item[0] == "pi36") {
			result += " PI36: <input type=checkbox class='pi36'";
			if (item[1] == "1")
				result += " checked='checked'";
			result += ">";
		}
	}

	return result;
}

var nRFSpeeds = ["250K", "1M"];

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

	var pi36 = 0;

	if ($cell.find("input.pi36").is(":checked"))
		pi36 = 1;

	return "device=" + value + ";pi36=" + pi36;;
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
	result += " PI36: <input type='checkbox' class='pi36'>";
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

		if (type == "RPIWS281X")
		{
			newRow += " onChange='alert(\"(Re)Enabling the RPIWS281x output may require an automatic reboot when the config is saved.\");'";
		}

		var countDisabled = "";

		if ((type == "Triks-C") ||
			(type == 'GPIO') ||
			(type == 'USBRelay') ||
			(type == 'RPIWS281X') ||
			(type == 'Pixelnet-Lynx') ||
			(type == 'Pixelnet-Open') ||
			(type == 'VirtualMatrix'))
			countDisabled = " disabled=''";

		newRow += "></td>" +
				"<td>" + type + "</td>" +
				"<td><input class='start' type=text size=6 maxlength=6 value='" + output[2] + "'></td>" +
				"<td><input class='count' type=text size=6 maxlength=6 value='" + output[3] + "'" + countDisabled + "></td>" +
				"<td>";

		if ((type == "DMX-Pro") ||
			(type == "DMX-Open") ||
			(type == "Pixelnet-Lynx") ||
			(type == "Pixelnet-Open")) {
			newRow += USBDeviceConfig(output[4]);
		} else if (type == "GenericSerial") {
			newRow += GenericSerialConfig(output[4]);
		} else if (type == "Renard") {
			newRow += RenardOutputConfig(output[4]);
		} else if (type == "LOR") {
			newRow += LOROutputConfig(output[4]);
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
		} else if (type == "USBRelay") {
			newRow += USBRelayConfig(output[4]);
		} else if (type == "RPIWS281X") {
			newRow += RPIWS281XConfig(output[4]);
		} else if (type == "VirtualMatrix") {
			newRow += VirtualMatrixConfig(output[4]);
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
				DialogError("Save Channel Outputs", "Invalid Renard Config");
				return;
			}
			maxChannels = 1528;
		} else if (type == "LOR") {
			config += GetLOROutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid LOR Config");
				return;
			}
			maxChannels = 3840;
		} else if (type == "SPI-WS2801") {
			config += GetSPIOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid SPI-WS2801 Config");
				return;
			}
			maxChannels = 1530;
		} else if (type == "SPI-nRF24L01") {
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
				DialogError("Save Channel Outputs", "Invalid Triks-C Config");
				return;
			}
			maxChannels = 9216;
		} else if (type == "GPIO") {
			config += GetGPIOOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid GPIO Config");
				return;
			}
			maxChannels = 1;
		} else if (type == "GPIO-595") {
			config += GetGPIO595OutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid GPIO-595 Config");
				return;
			}
			maxChannels = 128;
		} else if (type == "GenericSerial") {
			config += GetGenericSerialConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Generic Serial Config");
				return;
			}
			maxChannels = 3072;
		} else if (type == "USBRelay") {
			config += GetUSBRelayOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid USBRelay Config");
				return;
			}
			maxChannels = 8;
		} else if (type == "RPIWS281X") {
			config += GetRPIWS281XOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid RPIWS281X Config");
				return;
			}
			// Two outputs with max 1000 pixels per output
			maxChannels = 6000;
		} else if (type == "VirtualMatrix") {
			config += GetVirtualMatrixOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Virtual Matrix Config");
				return;
			}
			maxChannels = 500000;
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
		if (endChannel > 524288) {
			DialogError("Save Channel Outputs",
				"Start Channel '" + startChannel + "' plus Channel Count '" + channelCount + "' exceeds 524288 on row " + rowNumber);
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
		row.find("td input.count").prop('disabled', true);
	} else if (type == "Renard") {
		config += NewRenardConfig();
		row.find("td input.count").val("286");
	} else if (type == "LOR") {
		config += NewLORConfig();
		row.find("td input.count").val("16");
		row.find("td input.speed").val("19200");
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
	} else if (type == "GenericSerial") {
		config += NewGenericSerialConfig();
		row.find("td input.count").val("1");
	} else if (type == "USBRelay") {
		config += NewUSBRelayConfig();
		row.find("td input.count").val("2");
		row.find("td input.count").prop('disabled', true);
	} else if (type == "RPIWS281X") {
		config += NewRPIWS281XConfig();
		row.find("td input.act").change(function() {
			alert("(Re)Enabling the RPIWS281x output may require an automatic reboot when the config is saved.");
			});
		row.find("td input.count").val("3");
		row.find("td input.count").prop('disabled', true);
	} else if ((type == "VirtualMatrix") || (type == "FBMatrix")) {
		config += NewVirtualMatrixConfig();
		row.find("td input.count").val("1536");
		row.find("td input.count").prop('disabled', true);
	}

	row.find("td:nth-child(6)").html(config);
}

function OtherTypeSelected(selectbox) {
	$row = $(selectbox.parentNode.parentNode);

	var type = $(selectbox).val();

	if ((Object.keys(SerialDevices).length == 0) &&
			((type == 'DMX-Pro') ||
			 (type == 'DMX-Open') ||
			 (type == 'LOR') ||
			 (type == 'Pixelnet-Lynx') ||
			 (type == 'Pixelnet-Open') ||
			 (type == 'Renard') ||
			 (type == 'Triks-C')))
	{
		DialogError("Add Output", "No available serial devices detected.  Do you have a USB Serial Dongle attached?");
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

	if ((type == 'Triks-C') || (type == 'GPIO'))
	{
		$row.find('input.count').prop('disabled', true);
	}

	$row.find("td:nth-child(3)").html(type);

	AddOtherTypeOptions($row, type);
}

function AddOtherOutput() {
	if ((Object.keys(SerialDevices).length == 0) &&
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
				"<option value='GenericSerial'>Generic Serial</option>" +
<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
				"<option value='GPIO'>GPIO</option>" +
				"<option value='GPIO-595'>GPIO-595</option>" +
<?
	}
?>
				"<option value='LOR'>LOR</option>" +
				"<option value='Pixelnet-Lynx'>Pixelnet-Lynx</option>" +
				"<option value='Pixelnet-Open'>Pixelnet-Open</option>" +
				"<option value='Renard'>Renard</option>" +
<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
				"<option value='SPI-WS2801'>SPI-WS2801</option>" +
				"<option value='SPI-nRF24L01'>SPI-nRF24L01</option>" +
				"<option value='RPIWS281X'>RPIWS281X</option>" +
<?
	}
?>
				"<option value='VirtualMatrix'>Virtual Matrix</option>" +
				"<option value='Triks-C'>Triks-C</option>" +
				"<option value='USBRelay'>USBRelay</option>" +
			"</select></td>" +
			"<td><input class='start' type='text' size=6 maxlength=6 value='' style='display: none;'></td>" +
			"<td><input class='count' type='text' size=6 maxlength=6 value='' style='display: none;'></td>" +
			"<td> </td>" +
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

/////////////////////////////////////////////////////////////////////////////
// BBB48String support functions
var BBB48StringOutputs = 48;

function GetBBB48StringRows()
{
	var subType = $('#BBB48StringSubType').val();
	var rows = 48;

	if (subType == 'F16-B')
		rows = 16;
	else if (subType == 'F16-B-32')
		rows = 32;
    else if (subType == 'F16-B-40')
        rows = 40;
	else if (subType == 'F16-B-48')
		rows = 48;
	else if (subType == 'F4-B')
		rows = 4;
    else if (subType == 'F8-B')
        rows = 12;
    else if (subType == 'F8-B-16')
        rows = 16;
    else if (subType == 'F8-B-20')
        rows = 20;
    else if (subType == 'F8-B-EXP')
        rows = 28;
    else if (subType == 'F8-B-EXP-32')
        rows = 32;
    else if (subType == 'F8-B-EXP-36')
        rows = 36;
    else if (subType == 'F32-B')
        rows = 40;
    else if (subType == 'F32-B-48')
        rows = 48;
    else if (subType == 'RGBCape24')
        rows = 24;
	else if (subType == 'RGBCape48C')
		rows = 48;
    else if (subType == 'RGBCape48F')
        rows = 48;

	return rows;
}

function GetBBB48StringConfig()
{
	var config = new Object();
	var startChannel = 999999;
	var endChannel = 1;

	config.type = "BBB48String";
	config.enabled = 0;
	config.subType = $('#BBB48StringSubType').val();
	config.startChannel = 0;
	config.channelCount = 0;
	config.outputCount = BBB48StringOutputs;
	config.outputs = [];

	if ($('#BBB48StringEnabled').is(":checked"))
		config.enabled = 1;

	var i = 0;
	for (i = 0; i < BBB48StringOutputs; i++)
	{
		var output = new Object();
		var ai = '\\[' + i + '\\]';

		output.portNumber = i;
		output.startChannel = parseInt($('#BBB48StartChannel' + ai).val());
		output.pixelCount = parseInt($('#BBB48PixelCount' + ai).val());
		output.colorOrder = $('#BBB48ColorOrder' + ai).val();
		output.nullNodes = parseInt($('#BBB48NullNodes' + ai).val());
		output.hybridMode = 0;
		output.reverse = parseInt($('#BBB48Direction' + ai).val());
		output.grouping = parseInt($('#BBB48Grouping' + ai).val());
		output.zigZag = parseInt($('#BBB48ZigZag' + ai).val());
        output.brightness = parseInt($('#BBB48Brightness' + ai).val());
        output.gamma = parseFloat($('#BBB48Gamma' + ai).val());
        
		if ($('#BBB48HybridMode' + ai).is(":checked"))
			output.hybridMode = 1;

		if (output.startChannel < startChannel)
			startChannel = output.startChannel;

		var maxOutputChannel = output.startChannel + ((output.nullNodes + output.pixelCount) * 3) - 1;
		if (maxOutputChannel > endChannel)
			endChannel = maxOutputChannel;

		config.outputs.push(output);
	}

	config.startChannel = startChannel;
	config.channelCount = endChannel - startChannel + 1;

	return config;
}

function GetColorOptionsSelect(id, selected)
{
	var options = ["RGB", "RBG", "GRB", "GBR", "BRG", "BGR"];
	var html = "";
	html += "<select id='" + id + "'>";

	var i = 0;
	for (i = 0; i < options.length; i++)
	{
		html += "<option value='" + options[i] + "'";

		if (options[i] == selected)
			html += " selected";

		html += ">" + options[i] + "</option>";
	}
	html += "</select>";

	return html;
}

function GetDirectionOptionsSelect(id, selected)
{
	var html = "";

    html += "<select id='" + id + "'>";
    
    html += "<option value='0'";
    if (selected == 0)
        html += " selected";

    html += ">Forward</option>";

    html += "<option value='1'";
    if (selected == 1)
        html += " selected";
    
    html += ">Reverse</option>";

    html += "</select>";
	return html;
}
function GetBrightnessOptionsSelect(id, selected)
{
    var html = "";
    html += "<select id='" + id + "'>";
    
    var i = 0;
    for (i = 100; i >= 0; )
    {
        html += "<option value='" + i + "'";
        if (selected == i)
            html += " selected";
        html += ">" + i + "%</option>";
        i = i - 5;
    }
    
    
    html += "</select>";
    return html;
}

function UpdateBBBStringEndChannel(row)
{
	var p = parseInt($('#BBB48PixelCount\\[' + row + '\\]').val());
	var sc = parseInt($('#BBB48StartChannel\\[' + row + '\\]').val());
	var gc = parseInt($('#BBB48Grouping\\[' + row + '\\]').val());

	if (gc == 0)
		gc = 1;

	$('#BBB48EndChannel\\[' + row + '\\]').val(sc - 1 + ((p / gc) * 3));
}

function DrawBBB48StringTable()
{
	var html = "";

	BBB48StringOutputs = GetBBB48StringRows();

	var subType = $('#BBB48StringSubType').val();

	if ((subType == 'F16-B-48')
        || (subType == 'F8-B-20')
        || (subType == 'F8-B-EXP-36')
        || (subType == 'F32-B-48')
        || (subType == 'RGBCape24')
        || (subType == 'RGBCape48C')
        || (subType == 'RGBCape48F'))
	{
		$('#BBBSerialSelect').hide();
		$('#BBBSerialOutputs').hide();
	}
	else
	{
		$('#BBBSerialSelect').show();
		$('#BBBSerialOutputs').show();
	}

	var s = 0;
	for (s = 0; s < BBB48StringOutputs; s++)
	{
        if (subType == 'F8-B-EXP') {
            if (s == 12 || s == 8) {
                html += "<tr><td colspan='12'><hr></td></tr>\n";
            }
        } else if (subType == 'F8-B-EXP-32' && (s == 16 || s == 12 || s == 8)) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F8-B-EXP-36') {
            if (s == 20 || s == 16 || s == 12 || s == 8) {
                html += "<tr><td colspan='12'><hr></td></tr>\n";
            }
        } else if (subType == 'F8-B-20' && (s == 16 || s == 12 || s == 8)) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F8-B-16' && (s == 12 || s == 8)) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F8-B' && s == 8) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F32-B' && s == 36) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F32-B-48' && (s == 36 || s == 40 || s == 44) ) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (s && ((s % 16) == 0)) {
    		html += "<tr><td colspan='12'><hr></td></tr>\n";
		}

		html += "<tr id='BBB48StringRow" + s + "'>";
		html += "<td class='center'>" + (s+1) + "</td>";

		if (!("BBB48String" in channelOutputsLookup))
		{
			channelOutputsLookup["BBB48String"] = new Object();
			channelOutputsLookup["BBB48String"].outputs = [];
		}

		if (typeof channelOutputsLookup["BBB48String"].outputs[s] == 'undefined')
		{
			channelOutputsLookup["BBB48String"].outputs[s] = new Object();
			channelOutputsLookup["BBB48String"].outputs[s].portNumber = s;
			channelOutputsLookup["BBB48String"].outputs[s].startChannel = 1;
			channelOutputsLookup["BBB48String"].outputs[s].pixelCount = 0;
			channelOutputsLookup["BBB48String"].outputs[s].colorOrder = "RGB";
			channelOutputsLookup["BBB48String"].outputs[s].nullNodes = 0;
			channelOutputsLookup["BBB48String"].outputs[s].hybridMode = 0;
			channelOutputsLookup["BBB48String"].outputs[s].reverse = 0;
			channelOutputsLookup["BBB48String"].outputs[s].grouping = 0;
			channelOutputsLookup["BBB48String"].outputs[s].zigZag = 0;
            channelOutputsLookup["BBB48String"].outputs[s].brightness = 100;
            channelOutputsLookup["BBB48String"].outputs[s].gamma = 1.0;
		}

		var p = channelOutputsLookup["BBB48String"].outputs[s].pixelCount;
		var sc = channelOutputsLookup["BBB48String"].outputs[s].startChannel;
		var gc = channelOutputsLookup["BBB48String"].outputs[s].grouping;
        var bright = channelOutputsLookup["BBB48String"].outputs[s].brightness;
        var gamma = channelOutputsLookup["BBB48String"].outputs[s].gamma;

        if (bright == undefined) {
            bright = 100;
        }
        if (gamma == undefined) {
            gamma = 1.0;
        }

		if (gc == 0)
			gc = 1;

		var ec = sc - 1 + ((p / gc) * 3);

		html += "<td class='center'><input id='BBB48PixelCount[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].pixelCount + "' onChange='UpdateBBBStringEndChannel(" + s + ");'></td>";
		html += "<td class='center'><input id='BBB48StartChannel[" + s + "]' type='text' size='6' maxlength='6' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].startChannel + "' onChange='UpdateBBBStringEndChannel(" + s + ");'></td>";
		html += "<td class='center'><input id='BBB48EndChannel[" + s + "]' type='text' size='6' maxlength='6' value='" + ec + "' disabled=''></td>";
		html += "<td class='center'>" + GetColorOptionsSelect("BBB48ColorOrder[" + s + "]", channelOutputsLookup["BBB48String"].outputs[s].colorOrder) + "</td>";
		html += "<td class='center'>" + GetDirectionOptionsSelect("BBB48Direction[" + s + "]", channelOutputsLookup["BBB48String"].outputs[s].reverse) + "</td>";
		html += "<td class='center'><input id='BBB48Grouping[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].grouping + "' onChange='UpdateBBBStringEndChannel(" + s + ");'></td>";
		html += "<td class='center'><input id='BBB48NullNodes[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].nullNodes + "'></td>";

		html += "<td class='center'><input id='BBB48HybridMode[" + s + "]' type='checkbox'";
		if (channelOutputsLookup["BBB48String"].outputs[s].hybridMode)
			html += " checked";
		html += "></td>";

		html += "<td class='center'><input id='BBB48ZigZag[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].zigZag + "'></td>";
        
        
        html += "<td class='center'>" + GetBrightnessOptionsSelect("BBB48Brightness[" + s + "]", bright) + "</td>";
        html += "<td class='center'><input id='BBB48Gamma[" + s + "]' type='text' size='4' maxlength='4' value='" + gamma + "'></td>";


		html += "</tr>";
	}

	$('#BBB48StringTable tbody').html(html);
}

function BBB48StringSubTypeChanged()
{
	DrawBBB48StringTable();
	SetupBBBSerialStartChannels();
}
function BBB48SerialTypeChanged() {
    if ($('#BBBSerialMode').val() == 'DMX') {
        $('#DMXNumChannelOutput').show();
    } else {
        $('#DMXNumChannelOutput').hide();
    }
}

function InitializeBBB48String()
{
	if (("BBB48String-Enabled" in channelOutputsLookup) &&
			(channelOutputsLookup["BBB48String-Enabled"]))
		$('#BBB48StringEnabled').prop('checked', true);

	if (("BBB48String" in channelOutputsLookup) &&
		(typeof channelOutputsLookup["BBB48String"].subType != "undefined"))
	{
		if ((channelOutputsLookup["BBB48String"].subType == "F16-B") &&
				(channelOutputsLookup["BBB48String"].outputCount > 16))
		{
			if (channelOutputsLookup["BBB48String"].outputCount == 32)
				$('#BBB48StringSubType').val("F16-B-32");
			else if (channelOutputsLookup["BBB48String"].outputCount == 40)
				$('#BBB48StringSubType').val("F16-B-40");
			else
				$('#BBB48StringSubType').val("F16-B-48");
		}
		else
		{
			$('#BBB48StringSubType').val(channelOutputsLookup["BBB48String"].subType);
		}

	}

	DrawBBB48StringTable();
}

function InitializeBBBSerial()
{
	var maxPorts = 8;

	if (("BBB48String" in channelOutputsLookup) &&
		(typeof channelOutputsLookup["BBB48String"].subType != "undefined"))
	{
		if ((channelOutputsLookup["BBB48String"].subType == 'F16-B-48') ||
			(channelOutputsLookup["BBB48String"].subType == 'F8-B-20') ||
            (channelOutputsLookup["BBB48String"].subType == 'F8-B-EXP-36') ||
            (channelOutputsLookup["BBB48String"].subType == 'F32-B-48') ||
            (channelOutputsLookup["BBB48String"].subType == 'RGBCape24') ||
            (channelOutputsLookup["BBB48String"].subType == 'RGBCape48C') ||
            (channelOutputsLookup["BBB48String"].subType == 'RGBCape48F'))
			return; // nothing to setup if non-serial cape
	}

	if (("BBBSerial" in channelOutputsLookup) &&
		(typeof channelOutputsLookup["BBBSerial"].subType != "undefined") &&
		(channelOutputsLookup["BBBSerial"].subType != 'off'))
	{
		$('#BBBSerialMode').val(channelOutputsLookup["BBBSerial"].subType);
		var outputs = channelOutputsLookup["BBBSerial"].outputs;
        if (channelOutputsLookup["BBBSerial"].subType == 'DMX') {
            if (outputs[0].channelCount > 0 && outputs[0].channelCount < 513) {
                $('#BBBSerialNumDMXChannels').val(outputs[0].channelCount);
            } else {
                $('#BBBSerialNumDMXChannels').val("512");
            }
        } else {
            $('#BBBSerialNumDMXChannels').val("512");
        }

		for (var i = 0; i < outputs.length; i++)
		{
			var outputNumber = outputs[i].outputNumber + 1;
			$('#BBBSerialStartChannel' + outputNumber).val(outputs[i].startChannel);
		}
	}

	SetupBBBSerialStartChannels();
    BBB48SerialTypeChanged();
}

function SetupBBBSerialStartChannels()
{
	var subType = $('#BBB48StringSubType').val();

	if (subType == 'F4-B')
		maxPorts = 1;
    else if ((subType == 'F8-B-16') || (subType == 'F8-B-EXP-32'))
        maxPorts = 4;
	else
		maxPorts = 8;

	for (var i = 1; i <= 8; i++)
	{
		if (i <= maxPorts)
			$('#BBBSerialOutputRow' + i).show();
		else
			$('#BBBSerialOutputRow' + i).hide();
	}
}

function GetBBBSerialConfig()
{
	var config = new Object();
	var startChannel = 999999;
	var endChannel = 1;

	config.type = "BBBSerial";
	config.enabled = 0;
	config.subType = $('#BBBSerialMode').val();
	config.startChannel = 0;
	config.channelCount = 0;
    config.device = $('#BBB48StringSubType').val();
	config.outputs = [];

	if (config.subType != 'off')
		config.enabled = 1;
    
    if (!$('#BBB48StringEnabled').is(":checked"))
        config.enabled = 0;
  
    if ($('#BBBSerialSelect').is(":hidden"))
        config.enabled = 0;



	var i = 1;
	for (i = 1; i <= 8; i++)
	{
		var output = new Object();
		var ai = '\\[' + i + '\\]';

		output.outputNumber = i - 1;
		output.startChannel = parseInt($('#BBBSerialStartChannel' + i).val());

		if (output.startChannel < startChannel)
			startChannel = output.startChannel;

		output.outputType = config.subType;

		if (config.subType == 'DMX')
            output.channelCount = parseInt($('#BBBSerialNumDMXChannels').val());
		else
			output.channelCount = 4096;

		var maxOutputChannel = output.startChannel + output.channelCount - 1;
		if (maxOutputChannel > endChannel)
			endChannel = maxOutputChannel;

		config.outputs.push(output);
	}

	config.startChannel = startChannel;
	config.channelCount = endChannel - startChannel + 1;

	return config;
}

/////////////////////////////////////////////////////////////////////////////
// LED Panel Matrix support functions

<?
$LEDPanelOutputs = 12;
$LEDPanelPanelsPerOutput = 12;
$LEDPanelRows = 1;
$LEDPanelCols = 1;
$LEDPanelWidth = 32;
$LEDPanelHeight = 16;
$LEDPanelScan = 8;
$LEDPanelInterleave = 0;

if ($settings['Platform'] == "BeagleBone Black")
{
    if (strpos($settings['SubPlatform'], 'Green Wireless') !== FALSE) {
        $LEDPanelOutputs = 5;
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE) {
        $LEDPanelOutputs = 6;
    } else {
        $LEDPanelOutputs = 8;
    }
    
	$LEDPanelPanelsPerOutput = 12;
}

$maxLEDPanels = $LEDPanelOutputs * $LEDPanelPanelsPerOutput;
$maxLEDPanels = 64; // Override to allow different panel configs using Linsn/ColorLight cards

if (isset($settings['LEDPanelsLayout']))
{
	$parts = explode('x', $settings['LEDPanelsLayout']);
	if (count($parts) == 2)
	{
		$LEDPanelCols = $parts[0];
		$LEDPanelRows = $parts[1];
	}
}

function printLEDPanelLayoutSelect()
{
	global $maxLEDPanels;
	$values = array();

	for ($r = 1; $r <= $maxLEDPanels; $r++)
	{
		for ($c = 1; $c <= $maxLEDPanels; $c++)
		{
			if (($r * $c) <= $maxLEDPanels)
			{
				$value = sprintf("%dx%d", $r, $c);
				$values[$value] = $value;
			}
		}
	}

	PrintSettingSelect("Panel Layout", "LEDPanelsLayout", 1, 0, "1x1", $values, "", "LEDPanelLayoutChanged");
}

function printLEDPanelSizeSelect($platform, $def, $interleave)
{
	$values = array();
    if ($platform == "BeagleBone Black") {
        $values["32x16 1/8 Scan"] = "32x16x8";
        $values["32x16 1/4 Scan"] = "32x16x4";
        $values["32x16 1/2 Scan"] = "32x16x2";
        $values["32x32 1/16 Scan"] = "32x32x16";
        $values["64x32 1/16 Scan"] = "64x32x16";
        
        $values["64x32 1/8 Scan"] = "64x32x8x64";
        $values["32x32 1/8 Scan"] = "32x32x8x32";
        $values["40x20 1/5 Scan"] = "40x20x5";
    } else {
        $values["32x16"] = "32x16x8";
        $values["32x32"] = "32x32x16";
    }
    if ($interleave != "0" && $interleave != "") {
        PrintSettingSelect("Panel Size", "LEDPanelsSize", 1, 0, $def + "x" + $interleave, $values, "", "LEDPanelLayoutChanged");
    } else {
        PrintSettingSelect("Panel Size", "LEDPanelsSize", 1, 0, $def, $values, "", "LEDPanelLayoutChanged");
    }
}

?>

var LEDPanelColorOrder = 'RGB';
var LEDPanelOutputs = <? echo $LEDPanelOutputs; ?>;
var LEDPanelPanelsPerOutput = <? echo $LEDPanelPanelsPerOutput; ?>;
var LEDPanelWidth = <? echo $LEDPanelWidth; ?>;
var LEDPanelHeight = <? echo $LEDPanelHeight; ?>;
var LEDPanelScan = <? echo $LEDPanelScan; ?>;
var LEDPanelInterleave = <? echo $LEDPanelInterleave; ?>;
var LEDPanelRows = <? echo $LEDPanelRows; ?>;
var LEDPanelCols = <? echo $LEDPanelCols; ?>;

function UpdatePanelSize()
{
	var size = $('#LEDPanelsSize').val();
	var sizeparts = size.split("x");
	LEDPanelWidth = parseInt(sizeparts[0]);
	LEDPanelHeight = parseInt(sizeparts[1]);
    LEDPanelScan = parseInt(sizeparts[2]);
    LEDPanelInterleave = parseInt(sizeparts[3]);
}

function LEDPanelOrientationClicked(id)
{
	var src = $('#' + id).attr('src');

	if (src == 'images/arrow_N.png')
		$('#' + id).attr('src', 'images/arrow_R.png');
	else if (src == 'images/arrow_R.png')
		$('#' + id).attr('src', 'images/arrow_U.png');
	else if (src == 'images/arrow_U.png')
		$('#' + id).attr('src', 'images/arrow_L.png');
	else if (src == 'images/arrow_L.png')
		$('#' + id).attr('src', 'images/arrow_N.png');
}

function GetLEDPanelNumberSetting(id, key, maxItems, selectedItem)
{
	var html = "";
	var i = 0;
	var o = 0;
	var selected = "";

	html = "<select id='" + key + "'>";
	for (i = 0; i < maxItems; i++)
	{
		selected = "";

		if (i == selectedItem)
			selected = "selected";

		html += "<option value='" + i + "' " + selected + ">" + id + "-" + (i+1) + "</option>";
	}
	html += "</select>";

	return html;
}

function LEDPanelLayoutChanged()
{
	var layout = $('#LEDPanelsLayout').val();
	var parts = layout.split("x");
	LEDPanelCols = parseInt(parts[0]);
	LEDPanelRows = parseInt(parts[1]);
    LEDPanelScan = parseInt(parts[2]);
    LEDPanelInterleave = parseInt(parts[3]);

	UpdatePanelSize();

	var channelCount = LEDPanelCols * LEDPanelRows * LEDPanelWidth * LEDPanelHeight * 3;
	$('#LEDPanelsChannelCount').html(channelCount);

	DrawLEDPanelTable();
}

function DrawLEDPanelTable()
{
	var r;
	var c;
	var html = "";
	var key = "";

	for (r = 0 ; r < LEDPanelRows; r++)
	{
		html += "<tr>";
		for (c = 0; c < LEDPanelCols; c++)
		{
			html += "<td><table cellspacing=0 cellpadding=0><tr><td>";

			key = "LEDPanelOutputNumber_" + r + "_" + c;
			if (typeof channelOutputsLookup[key] !== 'undefined') {
				html += GetLEDPanelNumberSetting("O", key, LEDPanelOutputs, channelOutputsLookup[key].outputNumber);
			} else {
				html += GetLEDPanelNumberSetting("O", key, LEDPanelOutputs, 0);
			}

			html += "<img src='images/arrow_";

			if (typeof channelOutputsLookup[key] !== 'undefined') {
				html += channelOutputsLookup[key].orientation;
			} else {
				html += "N";
			}

			html += ".png' height=17 width=17 id='LEDPanelOrientation_" + r + "_" + c + "' onClick='LEDPanelOrientationClicked(\"LEDPanelOrientation_" + r + "_" + c + "\");'><br>";

			key = "LEDPanelPanelNumber_" + r + "_" + c;
			if (typeof channelOutputsLookup[key] !== 'undefined') {
				html += GetLEDPanelNumberSetting("P", key, LEDPanelPanelsPerOutput, channelOutputsLookup[key].panelNumber);
			} else {
				html += GetLEDPanelNumberSetting("P", key, LEDPanelPanelsPerOutput, 0);
			}

			html += "</td></tr></table></td>\n";
		}
		html += "</tr>";
	}

	$('#LEDPanelTable tbody').html(html);
}

function InitializeLEDPanels()
{
	if (("LEDPanelMatrix-Enabled" in channelOutputsLookup) &&
			(channelOutputsLookup["LEDPanelMatrix-Enabled"]))
		$('#LEDPanelsEnabled').prop('checked', true);

	if ("LEDPanelMatrix" in channelOutputsLookup)
	{
		$('#LEDPanelsStartChannel').val(channelOutputsLookup["LEDPanelMatrix"].startChannel);
		$('#LEDPanelsChannelCount').html(channelOutputsLookup["LEDPanelMatrix"].channelCount);
		$('#LEDPanelsColorOrder').val(channelOutputsLookup["LEDPanelMatrix"].colorOrder);
		$('#LEDPanelsBrightness').val(channelOutputsLookup["LEDPanelMatrix"].brightness);
		$('#LEDPanelsConnection').val(channelOutputsLookup["LEDPanelMatrix"].subType);
		$('#LEDPanelsInterface').val(channelOutputsLookup["LEDPanelMatrix"].interface);
		$('#LEDPanelsSourceMacInput').val(channelOutputsLookup["LEDPanelMatrix"].sourceMAC);
<?
	if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black")
	{
?>
		$('#LEDPanelsWiringPinout').val(channelOutputsLookup["LEDPanelMatrix"].wiringPinout);
<?
	}
    if ($settings['Platform'] == "BeagleBone Black")
    {
?>
        $('#LEDPanelsColorDepth').val(channelOutputsLookup["LEDPanelMatrix"].panelColorDepth);
<?
    }
?>
		$('#LEDPanelsStartCorner').val(channelOutputsLookup["LEDPanelMatrix"].invertedData);

		if ((channelOutputsLookup["LEDPanelMatrix"].subType == 'ColorLight5a75') ||
			(channelOutputsLookup["LEDPanelMatrix"].subType == 'LinsnRv9'))
		{
			LEDPanelOutputs = 12;
		}
	}

	DrawLEDPanelTable();
}

function GetLEDPanelConfig()
{
	var config = new Object();
	var panels = "";
	var xOffset = 0;
	var yOffset = 0;
	var yDiff = 0;

	config.type = "LEDPanelMatrix";
<?
	if ($settings['Platform'] == "BeagleBone Black")
	{
		echo "config.subType = 'LEDscapeMatrix';\n";
	}
	else
	{
		echo "config.subType = 'RGBMatrix';\n";
	}
?>

	UpdatePanelSize();

	config.enabled = 0;
	config.startChannel = parseInt($('#LEDPanelsStartChannel').val());
	config.channelCount = parseInt($('#LEDPanelsChannelCount').html());
	config.colorOrder = $('#LEDPanelsColorOrder').val();
	config.brightness = parseInt($('#LEDPanelsBrightness').val());
	if (($('#LEDPanelsConnection').val() === "ColorLight5a75") || ($('#LEDPanelsConnection').val() === "LinsnRV9"))
	{
		config.subType = $('#LEDPanelsConnection').val();
		config.interface = $('#LEDPanelsInterface').val();
		if (($('#LEDPanelsConnection').val() === "LinsnRV9") && $('#LEDPanelsSourceMacInput').val() !== "00:00:00:00:00:00" && $('#LEDPanelsSourceMacInput').val() !== "")
		{
			config.sourceMAC = $('#LEDPanelsSourceMacInput').val();
		}
	}
<?
	if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black")
	{
?>
	config.wiringPinout = $('#LEDPanelsWiringPinout').val();
<?
	}
?>
<?
    if ($settings['Platform'] == "BeagleBone Black")
    {
?>
        config.panelColorDepth = parseInt($('#LEDPanelsColorDepth').val());
<?
    }
?>
    config.brightness = parseInt($('#LEDPanelsBrightness').val());
	config.invertedData = parseInt($('#LEDPanelsStartCorner').val());
	config.panelWidth = LEDPanelWidth;
	config.panelHeight = LEDPanelHeight;
    config.panelScan = LEDPanelScan;
    if (LEDPanelInterleave) {
        config.panelInterleave = LEDPanelInterleave;
    }
	config.panels = [];

	if ($('#LEDPanelsEnabled').is(":checked"))
		config.enabled = 1;

	for (r = 0; r < LEDPanelRows; r++)
	{
		xOffset = 0;

		for (c = 0; c < LEDPanelCols; c++)
		{
			var panel = new Object();
			var id = "";

			id = "#LEDPanelOutputNumber_" + r + "_" + c;
			panel.outputNumber = parseInt($(id).val());

			id = "#LEDPanelPanelNumber_" + r + "_" + c;
			panel.panelNumber = parseInt($(id).val());

			id = "#LEDPanelOrientation_" + r + "_" + c;
			var src = $(id).attr('src');

			panel.xOffset = xOffset;
			panel.yOffset = yOffset;

			if (src == 'images/arrow_N.png')
			{
				panel.orientation = "N";
				xOffset += LEDPanelWidth;
				yDiff = LEDPanelHeight;
			}
			else if (src == 'images/arrow_R.png')
			{
				panel.orientation = "R";
				xOffset += LEDPanelHeight;
				yDiff = LEDPanelWidth;
			}
			else if (src == 'images/arrow_U.png')
			{
				panel.orientation = "U";
				xOffset += LEDPanelWidth;
				yDiff = LEDPanelHeight;
			}
			else if (src == 'images/arrow_L.png')
			{
				panel.orientation = "L";
				xOffset += LEDPanelHeight;
				yDiff = LEDPanelWidth;
			}

			panel.row = r;
			panel.col = c;

			config.panels.push(panel);
		}
		yOffset += yDiff;
	}

	return config;
}


<?
function PopulateEthernetInterfaces()
{
	$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v usb0 | grep -v wlan")));
	foreach ($interfaces as $iface)
	{
		$iface = preg_replace("/:$/", "", $iface);
		echo "<option value='" . $iface . "'>" . $iface . "</option>";
	}
}
?>

function LEDPannelsConnectionChanged()
{
	if (($('#LEDPanelsConnection').val() === "ColorLight5a75") || ($('#LEDPanelsConnection').val() === "LinsnRV9")) {
		$('#LEDPanelsConnectionInterface').show();
		$('#LEDPanelsInterface').show();
		if ($('#LEDPanelsConnection').val() === "LinsnRV9") {
			$('#LEDPanelsSourceMac').show();
		}
		else
		{
			$('#LEDPanelsSourceMac').hide();
		}

		LEDPanelOutputs = 12;
	}
	else 
	{
		$('#LEDPanelsConnectionInterface').hide();
		$('#LEDPanelsInterface').hide();
		$('#LEDPanelsSourceMac').hide();

<?
if ($settings['Platform'] == "BeagleBone Black")
	echo "		LEDPanelOutputs = 8;\n";
else
	echo "		LEDPanelOutputs = 3;\n";
?>
	}

	DrawLEDPanelTable();
}

/////////////////////////////////////////////////////////////////////////////

function UpdateChannelOutputLookup()
{
	var i = 0;
	for (i = 0; i < channelOutputs.channelOutputs.length; i++)
	{
		channelOutputsLookup[channelOutputs.channelOutputs[i].type + "-Enabled"] = channelOutputs.channelOutputs[i].enabled;
		if (channelOutputs.channelOutputs[i].type == "LEDPanelMatrix")
		{
			channelOutputsLookup["LEDPanelMatrix"] = channelOutputs.channelOutputs[i];

			var p = 0;
			for (p = 0; p < channelOutputs.channelOutputs[i].panels.length; p++)
			{
				var r = channelOutputs.channelOutputs[i].panels[p].row;
				var c = channelOutputs.channelOutputs[i].panels[p].col;

				channelOutputsLookup["LEDPanelOutputNumber_" + r + "_" + c]
					= channelOutputs.channelOutputs[i].panels[p];
				channelOutputsLookup["LEDPanelPanelNumber_" + r + "_" + c]
					= channelOutputs.channelOutputs[i].panels[p];
			}
		}
		else if (channelOutputs.channelOutputs[i].type == "BBB48String")
		{
			channelOutputsLookup["BBB48String"] = channelOutputs.channelOutputs[i];
		}
		else if (channelOutputs.channelOutputs[i].type == "BBBSerial")
		{
			channelOutputsLookup["BBBSerial"] = channelOutputs.channelOutputs[i];
		}
	}
}

function GetChannelOutputConfig()
{
	var config = new Object();

	config.channelOutputs = [];

	config.channelOutputs.push(GetLEDPanelConfig());

<?
	if ($settings['Platform'] == "BeagleBone Black")
	{
		echo "// BBB 48 String output\n";
		echo "config.channelOutputs.push(GetBBB48StringConfig());\n";
		echo "config.channelOutputs.push(GetBBBSerialConfig());\n";
	}
?>

  var result = JSON.stringify(config);
	return result;
}

function SaveChannelOutputsJSON()
{
	var configStr = GetChannelOutputConfig();

	var postData = "command=setChannelOutputsJSON&data=" + JSON.stringify(configStr);

	$.post("fppjson.php", postData
	).success(function(data) {
		$.jGrowl(" Channel Output configuration saved");
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Channel Output Config", "Save Failed");
	});
}

/////////////////////////////////////////////////////////////////////////////

<?

$channelOutputs = array();
$channelOutputsJSON = "";
if (file_exists($settings['channelOutputsJSON']))
{
	$channelOutputsJSON = file_get_contents($settings['channelOutputsJSON']);
	$channelOutputsJSON = preg_replace("/\n|\r/", "", $channelOutputsJSON);
	$channelOutputsJSON = preg_replace("/\"/", "\\\"", $channelOutputsJSON);
}

?>


$(document).ready(function(){
	var channelOutputsJSON = "<? echo $channelOutputsJSON; ?>";

	if (channelOutputsJSON != "")
		channelOutputs = jQuery.parseJSON(channelOutputsJSON);
	else
		channelOutputs.channelOutputs = [];

	UpdateChannelOutputLookup();

	// E1.31 initialization
	InitializeUniverses();
	getUniverses('TRUE');

<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
		echo "getPixelnetDMXoutputs('TRUE');\n";
	}

	if ($settings['Platform'] == "BeagleBone Black")
	{
		// BBB 48 String initialization
		echo "InitializeBBB48String();\n";
		echo "InitializeBBBSerial();\n";
	}
?>

	// 'Other' Channel Outputs initialization
	SetupSelectableTableRow(otherTableInfo);
	GetChannelOutputs();

	InitializeLEDPanels();
	LEDPannelsConnectionChanged();

	// Init tabs
  $tabs = $("#tabs").tabs({cache: true, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });
	var total = $tabs.find('.ui-tabs-nav li').length;
	var currentLoadingTab = 1;
	$tabs.bind('tabsload',function(){
		currentLoadingTab++;
		if (currentLoadingTab < total)
			$tabs.tabs('load',currentLoadingTab);
		else
			$tabs.unbind('tabsload');
	}).tabs('load',currentLoadingTab);

});

</script>
<!-- FIXME, move this to CSS to standardize the UI -->
<style>
.tblheader{
    background-color:#CCC;
    text-align:center;
    }
.tblheader td {
    border: solid 2px #888888;
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
				<li><a href="#tab-e131">E1.31 / ArtNet</a></li>
<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
				<li><a href="#tab-fpd">Falcon Pixelnet/DMX</a></li>
<?
	}
?>
<!--
				<li><a href="channeloutput_f16v2.php">F16 v2</a></li>
-->
<?
	$LEDPanelType = "RGBMatrix";
	if ($settings['Platform'] == "BeagleBone Black")
	{
		$LEDPanelType = "LEDscape/Octoscroller";
		echo "<li><a href='#tab-BBB48String'>BBB</a></li>\n";
	}
?>
				<li><a href='#tab-LEDPanels'>LED Panels</a></li>
				<li><a href="#tab-other">Other</a></li>
			</ul>

<!-- --------------------------------------------------------------------- -->

			<div id='tab-e131'>
				<div id='divE131'>
					<fieldset class="fs">
						<legend> E1.31 / ArtNet </legend>
						<div id='divE131Data'>

  <div style="overflow: hidden; padding: 10px;">
	<b>Enable E1.31 /ArtNet Output:</b> <? PrintSettingCheckbox("E1.31 / ArtNet Output", "E131Enabled", 1, 0, "1", "0"); ?><br><br>
	E1.31 / ArtNet Interface: <select id="selE131interfaces" onChange="SetE131interface();"><? PopulateInterfaces(); ?></select>
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

						</div>
					</fieldset>
				</div>
			</div>

<!-- --------------------------------------------------------------------- -->

<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
			<div id='tab-fpd'>
				<div id='divFPD'>
					<fieldset class="fs">
						<legend> Falcon Pixelnet/DMX (FPD) </legend>
						<div id='divFPDData'>
							<div style="overflow: hidden; padding: 10px;">
								<b>Enable FPD Output:</b> <? PrintSettingCheckbox("FPD Output", "FPDEnabled", 1, 0, "1", "0"); ?><br>
								<b>FPD Start Channel Offset:</b> <? PrintSettingText("FPDStartChannelOffset", 1, 0, 6, 6); ?> <font size=-1>(default is 0)</font><br>
								<br>
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
				</div>
			</fieldset>
		</div>
	</div>
<?
	}
?>

<!-- --------------------------------------------------------------------- -->

	<div id='tab-LEDPanels'>
		<div id='divLEDPanels'>
			<fieldset class="fs">
				<legend> <? echo $LEDPanelType; ?> LED Panels </legend>
				<div id='divLEDPanelsData'>
					<div style="overflow: hidden; padding: 10px;">
						<table border=0 cellspacing=3>
							<tr>
								<td><b>Enable LED Panel Output:</b></td><td><input id='LEDPanelsEnabled' type='checkbox'></td>
								<td width=20>&nbsp;</td>
								<td>&nbsp;</td>
							</tr>
							<tr>
								<td><b>Panel Layout (WxH):</b></td><td><? printLEDPanelLayoutSelect(); ?></td>
								<td>&nbsp;</td>
								<td><b>Start Channel:</b></td><td><input id='LEDPanelsStartChannel' type=text size=6 maxlength=6 value='1'></td>
							</tr>
							<tr>
								<td><b>Single Panel Size (WxH):</b></td><td><? printLEDPanelSizeSelect($settings['Platform'], $LEDPanelWidth + "x" + $LEDPanelHeight + "x" + $LEDPanelScan, $LEDPanelInterleave); ?></td>
								<td>&nbsp;</td>
								<td><b>Channel Count:</b></td><td><span id='LEDPanelsChannelCount'>1536</span></td>
							</tr>
							<tr>
								<td><b>Model Start Corner:</b></td><td>
									<select id='LEDPanelsStartCorner'>
										<option value='0'>Top Left</option>
										<option value='1'>Bottom Left</option>
									</select>
								</td>
								<td>&nbsp;</td>
								<td><b>Panel Color Order:</b></td><td>
									<select id='LEDPanelsColorOrder'>
										<option value='RGB'>RGB</option>
										<option value='RBG'>RBG</option>
										<option value='GRB'>GRB</option>
										<option value='GBR'>GBR</option>
										<option value='BRG'>BRG</option>
										<option value='BGR'>BGR</option>
									</select>
									</td>
							</tr>
							<tr>
								<td><b>Brightness:</b></td><td>
									<select id='LEDPanelsBrightness'>
<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
		for ($x = 100; $x >= 10; $x -= 5)
			echo "<option value='$x'>$x%</option>\n";
	}
	else
	{
		for ($x = 10; $x >= 1; $x -= 1)
			echo "<option value='$x'>$x</option>\n";
	}
?>
									</select>
									</td>
								<td>&nbsp;</td>
								<td><b>Wiring Pinout:</b></td><td>
									<select id='LEDPanelsWiringPinout'>
<?
    if ($settings['Platform'] == "Raspberry Pi")
    {
?>
										<option value='Standard'>Standard</option>
										<option value='Classic'>Classic</option>
										<option value='Adafruit'>Adafruit</option>
<?
    } else if ($settings['Platform'] == "BeagleBone Black") {
        if (strpos($settings['SubPlatform'], 'Green Wireless') !== FALSE) {
?>
                                        <option value='v2'>v2.x</option>
<?
        } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE) {
?>
            <option value='PocketScroller1x'>PocketScroller</option>
<?
        } else {
?>
                                        <option value='v1'>Standard v1.x</option>
                                        <option value='v2'>v2.x</option>
<?
        }
    }
?>
							</tr>
<?
if ($settings['Platform'] == "BeagleBone Black") {
    ?>
    <tr><td><b>Color Depth:</b></td><td>
    <select id='LEDPanelsColorDepth'>
    <option value='8'>8 Bit</option>
    <option value='7'>7 Bit</option>
    <option value='6'>6 Bit</option>
    </select>
    </td></tr>
    <?
}
?>
							<tr>
								<td><b>Connection:</b></td><td>
									<select id='LEDPanelsConnection' onChange='LEDPannelsConnectionChanged();'>
<?
	if (($settings['Platform'] == "Raspberry Pi") ||
		($settings['Platform'] == "BeagleBone Black"))
	{
?>
										<option value='Hat-Cap-Cape'>Hat/Cap/Cape</option>
<?
	}
?>
										<option value='ColorLight5a75'>ColorLight</option>
										<option value='LinsnRV9'>Linsn</option>
									</select>
								<td>&nbsp;</td>
								<td id='LEDPanelsConnectionInterface'><b>Interface:</b></td><td>
									<select id='LEDPanelsInterface' type='hidden'>
									<? PopulateEthernetInterfaces(); ?>
									</select>
									</td>
							</tr>

        
        
        
							<tr id='LEDPanelsSourceMac'>
								<td><b>Source Mac:</b></td><td>
								<input id='LEDPanelsSourceMacInput' type=text size=16 maxlength=17 value='00:00:00:00:00:00'>
								<td>&nbsp;</td>
							</tr>
							<tr>
								<td width = '70 px' colspan=5><input id='btnSaveChannelOutputsJSON' class='buttons' type='button' value='Save' onClick='SaveChannelOutputsJSON();'/> <font size=-1><? if ($settings['Platform'] == "BeagleBone Black") { echo "(this will save changes to BBB tab &amp; LED Panels tab)"; } ?></font></td>
							</tr>
						</table>
						<br>
						LED Panel Layout:<br>
						<table id='LEDPanelTable' border=1>
							<tbody>
							</tbody>
						</table>
						- O-# is physical output number.<br>
						- P-# is panel number on physical output.<br>
						- Arrow <img src='images/arrow_N.png' height=17 width=17> indicates panel orientation, click arrow to rotate.<br>
					</div>
				</div>
			</fieldset>
		</div>
	</div>

<!-- --------------------------------------------------------------------- -->

<?
if ($settings['Platform'] == "BeagleBone Black")
{
?>
	<div id='tab-BBB48String'>
		<div id='divBBB48String'>
			<fieldset class="fs">
				<legend> BeagleBone Black String Cape </legend>
				<div id='divBBB48StringData'>
					<div style="overflow: hidden; padding: 10px;">
						<table border=0 cellspacing=3>
							<tr>
								<td><b>Enable BBB String Cape:</b></td>
								<td><input id='BBB48StringEnabled' type='checkbox'></td>
								<td width=20>&nbsp;</td>
								<td width=20>&nbsp;</td>
								<td width=20>&nbsp;</td>
							</tr>
							<tr>
								<td><b>Cape Type:</b></td>
								<td colspan="3"><select id='BBB48StringSubType' onChange='BBB48StringSubTypeChanged();'>
									<option value='F16-B'>F16-B</option>
									<option value='F16-B-32'>F16-B w/ 32 outputs</option>
									<option value='F16-B-48'>F16-B w/ 48 outputs (No Serial)</option>
									<option value='F4-B'>F4-B</option>
                                    <option value='F8-B'>F8-B (8 serial)</option>
                                    <option value='F8-B-16'>F8-B (4 serial)</option>
                                    <option value='F8-B-20'>F8-B (No serial)</option>
                                    <option value='F8-B-EXP'>F8-B w/ Expansion (8 serial)</option>
                                    <option value='F8-B-EXP-32'>F8-B w/ Expansion (4 serial)</option>
                                    <option value='F8-B-EXP-36'>F8-B w/ Expansion (No serial)</option>
                                    <option value='F32-B'>F32-B</option>
                                    <option value='F32-B-48'>F32-B (No Serial)</option>
                                    <option value='RGBCape24'>RGBCape24</option>
									<option value='RGBCape48C'>RGBCape48C</option>
                                    <option value='RGBCape48F'>RGBCape48F</option>
									</select>
									</td>
							</tr>
							<tr id='BBBSerialSelect'>
								<td><b>BBB Serial Cape Mode:</b></td>
								<td><select id='BBBSerialMode' onChange='BBB48SerialTypeChanged();'>
										<option value='off'>Disabled</option>
										<option value='DMX'>DMX</option>
										<option value='Pixelnet'>Pixelnet</option>
									</select>
									</td>
								<td width=20>&nbsp;</td>
                                <td><div id="DMXNumChannelOutput">Num&nbsp;DMX&nbsp;Channels:&nbsp;<input id='BBBSerialNumDMXChannels' size='6' maxlength='6' value='512'></div></td>
							</tr>
							<tr>
								<td width = '70 px' colspan=5><input id='btnSaveChannelOutputsJSON' class='buttons' type='button' value='Save' onClick='SaveChannelOutputsJSON();'/> <font size=-1>(this will save changes to BBB tab &amp; LED Panels tab)</font></td>
							</tr>
						</table>
						<br>
						String Outputs:<br>
						<table id='BBB48StringTable'>
							<thead>
								<tr class='tblheader'>
									<td width='5%'>#</td>
									<td width='10%'>Node<br>Count</td>
									<td width='10%'>Start<br>Channel</td>
									<td width='10%'>End<br>Channel</td>
									<td width='5%'>RGB<br>Order</td>
									<td width='8%'>Direction</td>
									<td width='8%'>Group<br>Count</td>
									<td width='8%'>Null<br>Nodes</td>
									<td width='8%'>Hybrid</td>
									<td width='10%'>Zig<br>Zag</td>
                                    <td width='8%'>Brightness</td>
                                    <td width='8%'>Gamma</td>
									</tr>
							</thead>
							<tbody>
							</tbody>
						</table>
						<br>
						<span id='BBBSerialOutputs'>
						Serial Outputs:<br>

						<table>
							<thead>
								<tr class='tblheader'>
									<td width='30px'>#</td>
									<td width='90px'>Start<br>Channel</td>
								</tr>
							</thead>
							<tr id='BBBSerialOutputRow1'><td>1</td>
								<td><input id='BBBSerialStartChannel1' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow2'><td>2</td>
								<td><input id='BBBSerialStartChannel2' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow3'><td>3</td>
								<td><input id='BBBSerialStartChannel3' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow4'><td>4</td>
								<td><input id='BBBSerialStartChannel4' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow5'><td>5</td>
								<td><input id='BBBSerialStartChannel5' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow6'><td>6</td>
								<td><input id='BBBSerialStartChannel6' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow7'><td>7</td>
								<td><input id='BBBSerialStartChannel7' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow8'><td>8</td>
								<td><input id='BBBSerialStartChannel8' size='6' maxlength='6' value='1'></td>
								</tr>
						</table>
						
						</span>
					</div>
				</div>
			</fieldset>
		</div>
	</div>
<?
}
?>

<!-- --------------------------------------------------------------------- -->

	<div id='tab-other'>
		<div id='divOther'>
			<fieldset class="fs">
				<legend> Other Outputs </legend>
				<div id='divOtherData'>
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
	<tr class='tblheader'><td>#</td><td>Active</td><td>Output Type</td><td>Start Ch.</td><td>Channel<br>Count</td><td>Output Config</td></tr>
</thead>
<tbody>
</tbody>
</table>
</form>
</div>
				</div>
			</fieldset>
		</div>
	</div>

<!-- --------------------------------------------------------------------- -->

</div>
</div>

<div id='debugOutput'>
</div>

<div id="dialog-panelLayout" title="panelLayout" style="display: none">
  <div id="layoutText">
  </div>
</div>

	<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
