<?php include 'co-other-modules.php';?>
<script>

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
function USBDeviceConfig(config) {
	var result = "";

	result += DeviceSelect(SerialDevices, config.device);

	return result;
}

function GetUSBOutputConfig(result, cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result.device = value;

	return result;
}

function NewUSBConfig() {
	var config = {};

	config.device = "";

	return USBDeviceConfig(config);
}

/////////////////////////////////////////////////////////////////////////////
// Virtual Matrix Output
//
// Framebuffer Devices
var FBDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if (preg_match("/^fb[0-9]+/", $fileName)) {
			echo "FBDevices['$fileName'] = '$fileName';\n";
		}
	}
?>

function VirtualMatrixLayoutChanged(item) {
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

function VirtualMatrixConfig(config) {
	var result = "";

	result += " Width: <input type='text' size='3' maxlength='3' class='width' value='" + config.width + "' onChange='VirtualMatrixLayoutChanged(this);'>" +
				" Height: <input type='text' size='3' maxlength='3' class='height' value='" + config.height + "' onChange='VirtualMatrixLayoutChanged(this);'>";

	result += VirtualMatrixColorOrderSelect(config.colorOrder);
	result += " Invert: <input type=checkbox class='invert'";
	if (config.invert)
		result += " checked='checked'";
	result += ">";
	result += DeviceSelect(FBDevices, config.device);

	return result;
}

function NewVirtualMatrixConfig() {
	var config = {};

	config.width = 32;
	config.height = 16;
	config.layout = "32x16";
	config.colorOrder = "RGB";
	config.invert = 0;
	config.device = "fb0";

	return VirtualMatrixConfig(config);
}

function GetVirtualMatrixOutputConfig(result, cell) {
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

	var device = $cell.find("select.device").val();

	if (device == "")
		return "fb0";

	var invert = 0;
	if ($cell.find("input.invert").is(":checked"))
		invert = 1;

	result.width = parseInt(width);
	result.height = parseInt(height);
	result.layout = width + "x" + height;
	result.colorOrder = colorOrder;
	result.invert = invert;
	result.device = device;

	return result;
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
	var result = "";


	result += DeviceSelect(SerialDevices, config.device);
	result += GenericSerialSpeedSelect(config.speed);
	result += " Header: <input type='text' size=10 maxlength=20 class='serialheader' value='" + config.header + "'>";
	result += " Footer: <input type='text' size=10 maxlength=20 class='serialfooter' value='" + config.footer + "'>";

	return result;
}

function NewGenericSerialConfig() {
	var config = {};

	config.device = "";
	config.speed = 9600;
	config.header = "";
	config.footer = "";

	return GenericSerialConfig(config);
}

function GetGenericSerialConfig(result, cell) {
	$cell = $(cell);
	var device = $cell.find("select.device").val();

	if (device == "")
		return "";

	var speed = $cell.find("select.speed").val();

	if (speed == "")
		return "";

	var header = $cell.find("input.serialheader").val();

	var footer = $cell.find("input.serialfooter").val();

	result.device = device;
	result.speed = parseInt(speed);
	result.header = header;
	result.footer = footer;

	return result;
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

function USBRelayConfig(config) {
	var result = "";

	result += DeviceSelect(SerialDevices, config.device) + "  ";
	result += USBRelaySubTypeSelect(config.subType);
	result += USBRelayCountSelect(config.relayCount);

	return result;
}

function NewUSBRelayConfig() {
	var config = {};

	config.device = "";
	config.subType = "ICStation";
	config.relayCount = 2;

	return USBRelayConfig(config);
}

function GetUSBRelayOutputConfig(result, cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result.device = value;

	value = $cell.find("select.subType").val();

	if (value == "")
		return "";

	result.subType = value;

	value = $cell.find("select.relayCount").val();

	if (value == "")
		return "";

	result.relayCount = parseInt(value);

	return result;
}


/////////////////////////////////////////////////////////////////////////////
// VirtualDisplay (Display Preview on HDMI Output)

function NewVirtualDisplayConfig() {
	var config = {};

	config.width = 1280;
	config.height = 1024;
	config.colorOrder = "BGR";
	config.pixelSize = 2;
	config.device = "fb0";

	return VirtualDisplayConfig(config);
}

function VirtualDisplayColorOrderSelect(colorOrder) {
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

function VirtualDisplayConfig(config) {
	var result = "";

	result += "Width: <input type=text class='width' size=4 maxlength=4 value='" + config.width + "'> ";
	result += "Height: <input type=text class='height' size=4 maxlength=4 value='" + config.height + "'> ";
	result += VirtualDisplayColorOrderSelect(config.colorOrder);
	result += "Pixel Size: <select class='pixelSize'>";
	for (i = 1; i <= 3; i++)
	{
		result += "<option value='" + i + "'";
        if (config.pixelSize == i)
			result += " selected";
		result += ">" + i + "</option>";
	}
	result += "</select>";
	result += DeviceSelect(FBDevices, config.device);

	return result;
}

function GetVirtualDisplayConfig(result, cell) {
	$cell = $(cell);

	var width = $cell.find("input.width").val();
	if (width == "")
		return "";

	result.width = parseInt(width);

	var height = $cell.find("input.height").val();
	if (height == "")
		return "";

	result.height = parseInt(height);

	var colorOrder = $cell.find("select.colorOrder").val();

	if (colorOrder == "")
		return "";

	result.colorOrder = colorOrder;

	var pixelSize = $cell.find("select.pixelSize").val();
	if (pixelSize == "")
		return "";

	result.pixelSize = parseInt(pixelSize);

	var device = $cell.find("select.device").val();
	if (device == "")
		return "";

	result.device = device;

	return result;
}

function NewHTTPVirtualDisplayConfig() {
    var config = {};

    config.width = 1280;
    config.height = 1024;
    config.pixelSize = 2;
    return HTTPVirtualDisplayConfig(config);
}
function HTTPVirtualDisplayConfig(config) {
    var result = "";

    result += "Width: <input type=text class='width' size=4 maxlength=4 value='" + config.width + "'> ";
    result += "Height: <input type=text class='height' size=4 maxlength=4 value='" + config.height + "'> ";
    result += "Pixel Size: <select class='pixelSize'>";
    for (i = 1; i <= 3; i++)
    {
        result += "<option value='" + i + "'";
        if (config.pixelSize == i)
            result += " selected";
        result += ">" + i + "</option>";
    }
    result += "</select>";
    return result;
}

function GetHTTPVirtualDisplayConfig(result, cell) {
    $cell = $(cell);

    var width = $cell.find("input.width").val();
    if (width == "")
        return "";

    result.width = parseInt(width);

    var height = $cell.find("input.height").val();
    if (height == "")
        return "";

    result.height = parseInt(height);
    
    var pixelSize = $cell.find("select.pixelSize").val();
    if (pixelSize == "")
        pixelSize = "1";
    result.pixelSize = parseInt(pixelSize);
    return result;
}

/////////////////////////////////////////////////////////////////////////////
// MAX7219Matrix (MAX7219 w/ 8x8 LED Panels)

function MAX7219MatrixChanged(item) {
	var panels = parseInt($(item).parent().parent().find("input.panels").val());
	var cpp = parseInt($(item).parent().parent().find("select.channelsPerPixel").val());

	var channels = panels * 64 * cpp;

	$(item).parent().parent().find("input.count").val(channels);
}

function NewMAX7219MatrixConfig() {
	var config = {};

	config.panels = 1;
	config.channelsPerPixel = 1;

	return MAX7219MatrixConfig(config);
}

function MAX7219MatrixConfig(config) {
	var result = "";

	result += "Panels: <input type=text class='panels' size=2 maxlength=2 value='" + config.panels + "' onChange='MAX7219MatrixChanged(this);'> ";
	result += "Channels Per Pixel: <select class='channelsPerPixel' onChange='MAX7219MatrixChanged(this);'>";
	result += "<option value='1'>1</option>";
//	result += "<option value='3'";
	if (config.channelsPerPixel == 3)
		result += " selected";
	result += ">3</option></select>";

	return result;
}

function GetMAX7219MatrixConfig(result, cell) {
	$cell = $(cell);

	var panels = $cell.find("input.panels").val();
	if (panels == "")
		return "";

	result.panels = parseInt(panels);

	var channelsPerPixel = $cell.find("select.channelsPerPixel").val();

	if (channelsPerPixel == "")
		return "";

	result.channelsPerPixel = parseInt(channelsPerPixel);

	return result;
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
	var result = "";

	result += DeviceSelect(SerialDevices, config.device) + "&nbsp;&nbsp;";
	result += RenardSpeedSelect(config.renardspeed);
	result += RenardParmSelect(config.renardparm);

	return result;
}

function GetRenardOutputConfig(result, cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result.device = value;

	value = $cell.find("select.renardspeed").val();

	if (value == "")
		return "";

	result.renardspeed = parseInt(value);

	value = $cell.find("select.renardparm").val();

	if (value == "")
		return "";

	result.renardparm = value;

	return result;
}

function NewRenardConfig() {
	var config = {};

	config.device = "";
	config.renardspeed = "";
	config.renardparm = "";

	return RenardOutputConfig(config);
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
	var result = "";

	result += DeviceSelect(SerialDevices, config.device) + "&nbsp;&nbsp;";
	result += LORSpeedSelect(config.speed);
    
    var val = config.firstControllerId;
    if (!val) {
        val = 1;
    }
    result += "&nbsp;&nbsp;First Controller ID: <input class='firstControllerId' style='opacity: 1' id='firstControllerId' type='number' value='" + val + "' min='1' max='240' />";
	return result;
}

function GetLOROutputConfig(result, cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	result.device = value;

    value = $cell.find("select.speed").val();

	if (value == "")
		return "";

	result.speed = parseInt(value);
    
    value = $cell.find("input.firstControllerId");
    var v2 = parseInt(value.val());
    
    result.firstControllerId = v2;

	return result;
}

function NewLORConfig() {
	var config = {};

	config.device = "";
	config.speed = "";

	return LOROutputConfig(config);
}


/////////////////////////////////////////////////////////////////////////////
// nRF/Komby
var nRFSpeeds = ["250K", "1M"];

function SPInRFDeviceConfig(config) {
	var result = "";

	result += nRFSpeedSelect(nRFSpeeds, config.speed);
	result += "Channel: <input class='channel' type='text' size=4 maxlength=4 value='" + config.channel + "'>";

	return result;
}

function GetnRFSpeedConfig(result, cell) {
	$cell = $(cell);
	var value = $cell.find("select.speed").val();

	if (value == "")
		return "";

	result.speed = parseInt(value);

	var channel = $cell.find("input.channel").val();
	if (!channel && (channel < 0 || channel > 125))
		return "";

	result.channel = parseInt(channel);

	return result;
}

function NewnRFSPIConfig() {
	var config = {};

	config.speed = "";
	config.channel = "";

	return SPInRFDeviceConfig(config);
}

/////////////////////////////////////////////////////////////////////////////
// 'Other' Channel Outputs misc. functions
function PopulateChannelOutputTable(data) {
	$('#tblOtherOutputs > tbody').html("");
    
    if (data) {
        for (var i = 0; i < data.channelOutputs.length; i++) {
            var output = data.channelOutputs[i];
            var type = output.type;

            var newRow = "<tr class='rowUniverseDetails'><td style='vertical-align:top'>" + (i + 1) + "</td>" +
                    "<td style='vertical-align:top'><input class='act' type=checkbox";

            if (output.enabled)
                newRow += " checked";

            var countDisabled = "";

            ///////used for new way
			let output_module = output_modules.find(obj => obj.typeName == type);
			///////

			if ((type == 'USBRelay') ||
                (type == 'Pixelnet-Lynx') ||
                (type == 'Pixelnet-Open') ||
                (type == 'MAX7219Matrix') ||
                (type == 'VirtualDisplay') ||
                (type == 'VirtualMatrix') ||
				(() => {
					///////used for new way
					if (output_module != undefined)
						return output_module.fixedChan;
					return false;
					})()
					///////
				)
                countDisabled = " disabled='disabled'";

			var typeFriendlyName = type;
			if (output_module != undefined)
					typeFriendlyName = output_module.typeFriendlyName;

			newRow += "></td>" +	
					"<td class='type' style='vertical-align:top'>" + typeFriendlyName + "<input class='type' type='hidden' name='type' value='" +type+ "'></td>" +
                    "<td style='vertical-align:top'><input class='start' type=text size=7 maxlength=7 value='" + output.startChannel + "'></td>" +
                    "<td style='vertical-align:top'><input class='count' type=text size=7 maxlength=7 value='" + output.channelCount + "'" + countDisabled + "></td>" +
                    "<td style='vertical-align:top' class='config'>";

            if ((type == "DMX-Pro") ||
                (type == "DMX-Open") ||
                (type == "Pixelnet-Lynx") ||
                (type == "Pixelnet-Open")) {
                newRow += USBDeviceConfig(output);
            } else if (type == "GenericSerial") {
                newRow += GenericSerialConfig(output);
            } else if (type == "Renard") {
                newRow += RenardOutputConfig(output);
            } else if (type == "LOR") {
                newRow += LOROutputConfig(output);
            } else if (type == "SPI-nRF24L01") {
                newRow += SPInRFDeviceConfig(output);
            } else if (type == "VirtualDisplay") {
                newRow += VirtualDisplayConfig(output);
            } else if (type == "HTTPVirtualDisplay") {
                newRow += HTTPVirtualDisplayConfig(output);
            } else if (type == "MAX7219Matrix") {
                newRow += MAX7219MatrixConfig(output);
            } else if (type == "USBRelay") {
                newRow += USBRelayConfig(output);
            } else if (type == "VirtualMatrix") {
                newRow += VirtualMatrixConfig(output);
            } else if (output_module != undefined){
				///////new way
				newRow += output_module.PopulateHTMLRow(output);
			}

            newRow += "</td>" +
                    "</tr>";

            $('#tblOtherOutputs > tbody').append(newRow);

        }
    }
}

function GetChannelOutputs() {
	$.getJSON("fppjson.php?command=getChannelOutputs&file=co-other", function(data) {
		PopulateChannelOutputTable(data);
	});
}

function SaveOtherChannelOutputs() {
	var postData = {};
	var dataError = 0;
	var rowNumber = 1;
	var config;
	var outputs = [];

	$('#tblOtherOutputs > tbody > tr.rowUniverseDetails').each(function() {
		$this = $(this);

		var enabled = 0;

		if ($this.find("td:nth-child(2) input.act").is(":checked")) {
			enabled = 1;
		}

		// Type
		var type = $this.find("input.type").val();

		// User has not selected a type yet
		if (type.indexOf("None Selected") >= 0) {
			DialogError("Save Channel Outputs",
				"Output type must be selected on row " + rowNumber);
			dataError = 1;
			return;
		}

		var channelCount = $this.find("td:nth-child(5) input").val();
		if ((channelCount == "") || isNaN(channelCount) || (parseInt(channelCount) > maxChannels)) {
			DialogError("Save Channel Outputs",
				"Invalid Channel Count '" + channelCount + "' on row " + rowNumber);
			dataError = 1;
			return;
		}

		var startChannel = $this.find("td:nth-child(4) input").val();
		if ((startChannel == "") || isNaN(startChannel)) {
			DialogError("Save Channel Outputs",
				"Invalid Start Channel '" + startChannel + "' on row " + rowNumber);
			dataError = 1;
			return;
		}

		var endChannel = parseInt(startChannel) + parseInt(channelCount) - 1;
		if (endChannel > FPPD_MAX_CHANNELS) {
			DialogError("Save Channel Outputs",
				"Start Channel '" + startChannel + "' plus Channel Count '" + channelCount + "' exceeds " + FPPD_MAX_CHANNELS + " on row " + rowNumber);
			dataError = 1;
			return;
		}

		config = {};
		config.enabled = enabled;
		config.type = type;
		config.startChannel = parseInt(startChannel);
		config.channelCount = parseInt(channelCount);

		// Get output specific options
		var maxChannels = 510;
		if ((type == "DMX-Pro") ||
			(type == "DMX-Open") ||
			(type == "Pixelnet-Lynx") ||
			(type == "Pixelnet-Open")) {
			config = GetUSBOutputConfig(config, $this.find("td:nth-child(6)"));

			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Output Config");
				return;
			}
			maxChannels = 512;
			if (type.substring(0,8) == "Pixelnet")
				maxChannels = 4096;
		} else if (type == "Renard") {
			config = GetRenardOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Renard Config");
				return;
			}
			maxChannels = 1528;
		} else if (type == "LOR") {
			config = GetLOROutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid LOR Config");
				return;
			}
			maxChannels = 3840;
		} else if (type == "SPI-nRF24L01") {
			config = GetnRFSpeedConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid nRF Config");
				return;
			}
			maxChannels = 512;
		} else if (type == "VirtualDisplay") {
			config = GetVirtualDisplayConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Virtual Display Config");
				return;
			}
			maxChannels = FPPD_MAX_CHANNELS;
		} else if (type == "HTTPVirtualDisplay") {
			config = GetHTTPVirtualDisplayConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid HTTPVirtual Display Config");
				return;
			}
			maxChannels = FPPD_MAX_CHANNELS;
		} else if (type == "MAX7219Matrix") {
			config = GetMAX7219MatrixConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid MAX7219Matrix Config");
				return;
			}
			maxChannels = 1536;
		} else if (type == "GenericSerial") {
			config = GetGenericSerialConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Generic Serial Config");
				return;
			}
			maxChannels = 3072;
		} else if (type == "USBRelay") {
			config = GetUSBRelayOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid USBRelay Config");
				return;
			}
			maxChannels = 8;
		} else if (type == "VirtualMatrix") {
			config = GetVirtualMatrixOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Virtual Matrix Config");
				return;
			}
			maxChannels = 500000;
		} else if (output_modules.find(obj => obj.typeName == type) != undefined){
			///////new method
			let output_module = output_modules.find(obj => obj.typeName == type);
			config = output_module.GetOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid" + output_module.typeFriendlyName + "Config");
				return;
			}
			maxChannels = output_module.maxChannels;
		}


		outputs.push(config);

		rowNumber++;
	});

	if (dataError)
		return;

	postData.channelOutputs = outputs;

	// Double stringify so JSON in .json file is surrounded by { }
	var postDataStr = "command=setChannelOutputs&file=co-other&data=" + JSON.stringify(JSON.stringify(postData));

	$.post("fppjson.php", postDataStr).done(function(data) {
		PopulateChannelOutputTable(data);
		$.jGrowl("Channel Output Configuration Saved");
		SetRestartFlag(1);
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
	} else if (type == "SPI-nRF24L01") {
		config += NewnRFSPIConfig();
		row.find("td input.count").val("512");
	} else if (type == "VirtualDisplay") {
		config += NewVirtualDisplayConfig();
		row.find("td input.count").val(FPPD_MAX_CHANNELS);
		row.find("td input.count").prop('disabled', true);
    } else if (type == "HTTPVirtualDisplay") {
        config += NewHTTPVirtualDisplayConfig();
        row.find("td input.count").val(FPPD_MAX_CHANNELS);
        row.find("td input.count").prop('disabled', true);
	} else if (type == "MAX7219Matrix") {
		config += NewMAX7219MatrixConfig();
		row.find("td input.count").val("64");
		row.find("td input.count").prop('disabled', true);
	} else if (type == "GenericSerial") {
		config += NewGenericSerialConfig();
		row.find("td input.count").val("1");
	} else if (type == "USBRelay") {
		config += NewUSBRelayConfig();
		row.find("td input.count").val("2");
		row.find("td input.count").prop('disabled', true);
	} else if ((type == "VirtualMatrix") || (type == "FBMatrix")) {
		config += NewVirtualMatrixConfig();
		row.find("td input.count").val("1536");
		row.find("td input.count").prop('disabled', true);
	} else if (output_modules.find(obj => obj.typeName == type) != undefined) {
		///////new method
		let output_module = output_modules.find(obj => obj.typeName == type);
		config += output_module.AddNewRow();
		output_module.SetDefaults(row);
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
			 (type == 'Renard')))
	{
		$row.remove();
	}


	///////new way
	let output_module = output_modules.find(obj => obj.typeName == type);
	if (output_module != undefined) {
		if(!output_module.CanAddNewOutput()) {
			$row.remove();
		}
	}

	//add frindly type name if available
	var typeFriendlyName = type;
	if (output_module != undefined)
			typeFriendlyName = output_module.typeFriendlyName;
	$row.find("td.type").html(typeFriendlyName);
	$row.find("td.type").append("<input class='type' type='hidden' name='type' value='" +type+ "'>");

	AddOtherTypeOptions($row, type);
}

function AddOtherOutput() {
	var currentRows = $("#tblOtherOutputs > tbody > tr").length;

	var newRow = 
		"<tr class='rowUniverseDetails'><td style='vertical-align:top'>" + (currentRows + 1) + "</td>" +
			"<td style='vertical-align:top'><input class='act' type=checkbox></td>" +
			"<td style='vertical-align:top' class='type'><select id='outputType' class='type' onChange='OtherTypeSelected(this);'>" +
            "<option value=''>Select a type</option>";
 
 
	if (Object.keys(SerialDevices).length > 0) {
        newRow += "<option value='DMX-Pro'>DMX-Pro</option>" +
				"<option value='DMX-Open'>DMX-Open</option>" +
				"<option value='GenericSerial'>Generic Serial</option>" +
                "<option value='LOR'>LOR</option>" +
                "<option value='Pixelnet-Lynx'>Pixelnet-Lynx</option>" +
                "<option value='Pixelnet-Open'>Pixelnet-Open</option>" +
                "<option value='USBRelay'>USBRelay</option>" +
                "<option value='Renard'>Renard</option>";
    }
<?
	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
        if (Object.keys(SPIDevices).length > 0) {
            newRow += "<option value='SPI-nRF24L01'>SPI-nRF24L01</option>" +
                "<option value='MAX7219Matrix'>MAX7219 Matrix</option>";
        }
<?
	}
?>
        if (Object.keys(FBDevices).length > 0) {
            newRow += "<option value='VirtualMatrix'>Virtual Matrix</option>" +
                "<option value='VirtualDisplay'>Virtual Display</option>";
        }
        newRow += "<option value='HTTPVirtualDisplay'>HTTP Virtual Display</option>";
        newRow += "</select><input class='type' type='hidden' name='type' value='None Selected'></td>" +
			"<td style='vertical-align:top'><input class='start' type='text' size=7 maxlength=7 value='' style='display: none;'></td>" +
			"<td style='vertical-align:top'><input class='count' type='text' size=7 maxlength=7 value='' style='display: none;'></td>" +
			"<td> </td>" +
			"</tr>";

	$('#tblOtherOutputs > tbody').append(newRow);

	///////new method
	output_modules.forEach(function addOption(output_module) {
		$('#outputType').append(new Option(output_module.typeFriendlyName, output_module.typeName));
	})

}

var otherTableInfo = {
	tableName: "tblOtherOutputs",
	selected:  -1,
	enableButtons: [ "btnDeleteOther" ],
	disableButtons: []
	};

function RenumberColumns(tableName) {
	var id = 1;
	$('#' + tableName + ' > tbody > tr').each(function() {
		$this = $(this);
		$this.find("td:first").html(id);
		id++;
	});
}

function DeleteOtherOutput() {
	if (otherTableInfo.selected >= 0) {
		$('#tblOtherOutputs > tbody > tr:nth-child(' + (otherTableInfo.selected+1) + ')').remove();
		otherTableInfo.selected = -1;
		SetButtonState("#btnDeleteOther", "disable");
		RenumberColumns("tblOtherOutputs");
	}
}

$(document).ready(function(){
	SetupSelectableTableRow(otherTableInfo);
	GetChannelOutputs();
});
</script>

<div id='tab-other'>
	<div id='divOther'>
		<fieldset class="fs">
			<legend> Other Outputs </legend>
			<div id='divOtherData'>
				<div style="overflow: hidden; padding: 5px;">
					<form id="frmOtherOutputs">
						<input name="command" type="hidden" value="saveOtherOutputs" />
						<table>
							<tr><td width = "70 px"><input id="btnSaveOther" class="buttons" type="button" value = "Save" onClick='SaveOtherChannelOutputs();' /></td>
								<td width = "70 px"><input id="btnAddOther" class="buttons" type="button" value = "Add" onClick="AddOtherOutput();"/></td>
								<td width = "40 px">&nbsp;</td>
								<td width = "70 px"><input id="btnDeleteOther" class="disableButtons" type="button" value = "Delete" onClick="DeleteOtherOutput();"></td>
							</tr>
						</table>
                        <div class='fppTableWrapper'>
                            <div class='fppTableContents'>
                                <table id="tblOtherOutputs">
                                    <thead>
                                        <tr class='tblheader'>
                                            <th>#</th>
                                            <th>Active</th>
                                            <th>Output Type</th>
                                            <th>Start<br>Ch.</th>
                                            <th>Channel<br>Count</th>
                                            <th>Output Config</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                    </tbody>
                                </table>
                            </div>
                        </div>
					</form>
				</div>
			</div>
		</fieldset>
	</div>
</div>
