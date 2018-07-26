<script>
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

function VirtualMatrixConfig(config) {
	var result = "";

	result += " Width: <input type='text' size='3' maxlength='3' class='width' value='" + config.width + "' onChange='VirtualMatrixLayoutChanged(this);'>" +
				" Height: <input type='text' size='3' maxlength='3' class='height' value='" + config.height + "' onChange='VirtualMatrixLayoutChanged(this);'>";

	result += VirtualMatrixColorOrderSelect(config.colorOrder);

	return result;
}

function NewVirtualMatrixConfig() {
	var config = {};

	config.width = 32;
	config.height = 16;
	config.layout = "32x16";
	config.colorOrder = "RGB";

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

	result.width = parseInt(width);
	result.height = parseInt(height);
	result.layout = width + "x" + height;
	result.colorOrder = colorOrder;

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

	config.device = "ttyUSB0";
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
	var config = {};

	config.device = "";
	config.layout = "";

	return TriksDeviceConfig(config);
}

function TriksDeviceConfig(config) {
	var result = "";

	result += DeviceSelect(SerialDevices, config.device);
	result += TriksLayoutSelect(config.layout);

	return result;
}

function GetTriksOutputConfig(result, cell) {
	$cell = $(cell);
	var device = $cell.find("select.device").val();

	if (device == "")
		return "";

	var layout = $cell.find("select.layout").val();

	if (layout == "")
		return "";

	result.device = device;
	result.layout = layout;

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
// GPIO Pin direct high/low output
<?
if ($settings['Platform'] == "BeagleBone Black") {
    if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE) {
?>
var BeagleBoneHeaderPins = [
    "P1-02:87",
    "P1-04:89",
    "P1-06:5",
    "P1-08:2:P",
    "P1-10:3:P",
    "P1-12:4",
    "P1-20:20",
    "P1-26:12",
    "P1-28:13",
    "P1-29:117",
    "P1-30:43",
    "P1-31:114",
    "P1-32:42",
    "P1-33:111:P",
    "P1-34:26",
    "P1-35:88",
    "P1-36:110:P",
    "P2-01:50:P",
    "P2-02:59",
    "P2-03:23:P",
    "P2-04:58",
    "P2-05:30",
    "P2-06:57",
    "P2-07:31",
    "P2-08:60",
    "P2-09:15",
    "P2-10:52",
    "P2-11:14",
    "P2-17:65",
    "P2-18:47",
    "P2-19:27",
    "P2-20:64",
    "P2-22:46",
    "P2-24:44",
    "P2-25:41",
    "P2-27:40",
    "P2-28:116",
    "P2-29:7",
    "P2-30:113",
    "P2-31:19",
    "P2-32:112",
    "P2-33:45",
    "P2-34:115",
    "P2-35:86"
];
<?
    } else {
?>
var BeagleBoneHeaderPins = [
    "P8-03:38",
    "P8-04:39",
    "P8-05:34",
    "P8-06:35",
    "P8-07:66",
    "P8-08:67",
    "P8-09:69",
    "P8-10:68",
    "P8-11:45",
    "P8-12:44",
    "P8-13:23:P",
    "P8-14:26",
    "P8-15:47",
    "P8-16:46",
    "P8-17:27",
    "P8-18:65",
    "P8-19:22:P",
    "P8-20:63",
    "P8-21:62",
    "P8-22:37",
    "P8-23:36",
    "P8-24:33",
    "P8-25:32",
    "P8-26:61",
    "P8-27:86",
    "P8-28:88",
    "P8-29:87",
    "P8-30:89",
    "P8-31:10",
    "P8-32:11",
    "P8-33:9",
    "P8-34:81:P",
    "P8-35:8",
    "P8-36:80:P",
    "P8-37:78",
    "P8-38:79",
    "P8-39:76",
    "P8-40:77",
    "P8-41:74",
    "P8-42:75",
    "P8-43:72",
    "P8-44:73",
    "P8-45:70:P",
    "P8-46:71:P",
    "P9-11:30",
    "P9-12:60",
    "P9-13:31",
    "P9-14:50:P",
    "P9-15:48",
    "P9-16:51:P",
    "P9-17:5",
    "P9-18:4",
    "P9-19:13",
    "P9-20:12",
    "P9-21:3:P",
    "P9-22:2:P",
    "P9-23:49",
    "P9-24:15",
    "P9-25:117",
    "P9-26:14",
    "P9-27:115",
    "P9-28:113",
    "P9-29:111:P",
    "P9-30:112",
    "P9-31:110:P",
    "P9-41:20",
    "P9-91:116",
    "P9-42:7",
    "P9-92:114"
];
<?
    }
}
?>

function GPIOHeaderPinChanged(item) {
<?
    if ($settings['Platform'] == "BeagleBone Black") {
?>
        var itemVal = $(item).val();
        for (i = 0; i < BeagleBoneHeaderPins.length; i++) {
            var tmp = BeagleBoneHeaderPins[i].split(":");
            var opt = tmp[0];
            var val = tmp[1];
            if (val == itemVal) {
                var softPwm = $(item).parent().parent().find("input.softPWM");
                if (tmp.length == 3) {
                    softPwm.prop('disabled', false);
                } else {
                    softPwm.prop('disabled', true);
                    softPwm.prop('checked', false);
                }
            }
        }
<?
    }
?>
}
function GPIOGPIOSelect(currentValue) {
	var result = "";
<?
if ($settings['Platform'] == "Raspberry Pi") {
    if (isset($settings['PiFaceDetected']) && ($settings['PiFaceDetected'] == 1))
	{
?>
        var options = "4,5,6,12,13,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,200,201,202,203,204,205,206,207".split(",");
        result += " BCM GPIO Output: <select class='gpio'>";
<?
	}
	else
	{
?>
        var options = "4,5,6,12,13,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31".split(",");
        result += " BCM GPIO Output: <select class='gpio'>";
<?
	}
} else if ($settings['Platform'] == "BeagleBone Black") {
?>
    var options = BeagleBoneHeaderPins;
    result += "Header Pin: <select class='gpio' onChange='GPIOHeaderPinChanged(this)'>";
<?
}
?>

	var i = 0;
	for (i = 0; i < options.length; i++) {
		var opt = options[i];
        var val = opt;
        
<?
if ($settings['Platform'] == "BeagleBone Black") {
?>
        var tmp = opt.split(":");
        opt = tmp[0];
        val = tmp[1];
<?
}
?>
		result += "<option value='" + val + "'";
		if (currentValue == val)
			result += " selected";
		result += ">" + opt + "</option>";
	}

	result += "</select>";

	return result;
}

function NewGPIOConfig() {
	var config = {};

	config.gpio = "";
	config.invert = 0;
	config.softPWM = 0;

	return GPIODeviceConfig(config);
}

function GPIODeviceConfig(config) {
	var result = "";

	result += GPIOGPIOSelect(config.gpio);
	result += " Invert: <input type=checkbox class='invert'";
	if (config.invert)
		result += " checked='checked'";
	result += ">";

	result += " PWM: <input type=checkbox class='softPWM'";
	if (config.softPWM)
		result += " checked='checked'";
	result += ">";

    return result;
}

function GetGPIOOutputConfig(result, cell) {
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

    result.gpio = parseInt(gpio);
	result.invert = invert;
	result.softPWM = softPWM;

	return result;
}

/////////////////////////////////////////////////////////////////////////////
// GPIO-attached 74HC595 Shift Register Output

function GPIO595PrintOptionSelect(label, clss, options, currentValue) {
    var result = "";
    result += label;
    result += ": <select class='" + clss + "'>";
    var i = 0;
    for (i = 0; i < options.length; i++) {
        var tmp = options[i].split(":");
        var opt = tmp[0];
        var val = tmp[1];
        
        result += "<option value='" + val + "'";
        if (currentValue == val)
            result += " selected";
        result += ">" + opt + "</option>";
    }
    
    result += "</select>";
    return result;
}

function GPIO595GPIOSelect(currentValue) {
	var result = "";

    var vals = currentValue;
    if (Number.isInteger(vals)) {
        vals = (currentValue.toString() + "-0-0").split("-");
    } else {
        vals = currentValue.split("-");
    }
    if (vals.length < 3) {
        vals = "0-0-0".split("-");
    }
    var options;
    <?
    if ($settings['Platform'] == "Raspberry Pi") {
    ?>
        options = "17:17,18:18,22:22,23:23,24:24,27:27".split(",");
        result += "BCM GPIO Outputs&nbsp;nbsp;"
    <?
    } else if ($settings['Platform'] == "BeagleBone Black") {
    ?>
        options = BeagleBoneHeaderPins;
    <?
    }
    ?>

    result += GPIO595PrintOptionSelect("Clock", "clock", options, vals[0]);
    result += "&nbsp;&nbsp;";
    result += GPIO595PrintOptionSelect("Data", "data", options, vals[1]);
    result += "&nbsp;&nbsp;";
    result += GPIO595PrintOptionSelect("Latch", "latch", options, vals[2]);

	return result;
}

function NewGPIO595Config() {
	var config = {};

	config.gpio = "";

	return GPIO595DeviceConfig(config);
}

function GPIO595DeviceConfig(config) {
	var result = "";

	result += GPIO595GPIOSelect(config.gpio);

	return result;
}

function GetGPIO595OutputConfig(result, cell) {
	$cell = $(cell);

    var clock = $cell.find("select.clock").val();
    if (clock == "")
        return "";
    var data = $cell.find("select.data").val();
    if (data == "")
        return "";
    var latch = $cell.find("select.latch").val();
    if (latch == "")
        return "";

    result.gpio = clock + "-" + data + "-" + latch;

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
		if (config.channelsPerPixel == i)
			result += " selected";
		result += ">" + i + "</option>";
	}
	result += "</select>";

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

	return result;
}

function NewLORConfig() {
	var config = {};

	config.device = "";
	config.speed = "";

	return LOROutputConfig(config);
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
	var result = "";

	result += DeviceSelect(SPIDevices, config.device);
	result += " PI36: <input type=checkbox class='pi36'";
	if (config.pi36)
		result += " checked='checked'";

	result += ">";

	return result;
}

function NewSPIConfig() {
	var config = {};

	config.device = "";
	config.pi36 = 0;

	return SPIDeviceConfig(config);
}

function GetSPIOutputConfig(result, cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	var pi36 = 0;

	if ($cell.find("input.pi36").is(":checked"))
		pi36 = 1;

	result.device = value;
	result.pi36 = parseInt(pi36);

	return result;
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
	$('#tblOtherOutputs tbody').html("");
    
    if (data) {
        for (var i = 0; i < data.channelOutputs.length; i++) {
            var output = data.channelOutputs[i];
            var type = output.type;

            var newRow = "<tr class='rowUniverseDetails'><td>" + (i + 1) + "</td>" +
                    "<td><input class='act' type=checkbox";

            if (output.enabled)
                newRow += " checked";

            var countDisabled = "";

            if ((type == "Triks-C") ||
                (type == 'GPIO') ||
                (type == 'USBRelay') ||
                (type == 'Pixelnet-Lynx') ||
                (type == 'Pixelnet-Open') ||
                (type == 'MAX7219Matrix') ||
                (type == 'VirtualDisplay') ||
                (type == 'VirtualMatrix'))
                countDisabled = " disabled=''";

            newRow += "></td>" +
                    "<td>" + type + "</td>" +
                    "<td><input class='start' type=text size=6 maxlength=6 value='" + output.startChannel + "'></td>" +
                    "<td><input class='count' type=text size=6 maxlength=6 value='" + output.channelCount + "'" + countDisabled + "></td>" +
                    "<td>";

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
            } else if (type == "SPI-WS2801") {
                newRow += SPIDeviceConfig(output);
            } else if (type == "SPI-nRF24L01") {
                newRow += SPInRFDeviceConfig(output);
            } else if (type == "Triks-C") {
                newRow += TriksDeviceConfig(output);
            } else if (type == "GPIO") {
                newRow += GPIODeviceConfig(output);
            } else if (type == "GPIO-595") {
                newRow += GPIO595DeviceConfig(output);
            } else if (type == "VirtualDisplay") {
                newRow += VirtualDisplayConfig(output);
            } else if (type == "MAX7219Matrix") {
                newRow += MAX7219MatrixConfig(output);
            } else if (type == "USBRelay") {
                newRow += USBRelayConfig(output);
            } else if (type == "VirtualMatrix") {
                newRow += VirtualMatrixConfig(output);
            }

            newRow += "</td>" +
                    "</tr>";

            $('#tblOtherOutputs tbody').append(newRow);
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

	$('#tblOtherOutputs tbody tr').each(function() {
		$this = $(this);

		var enabled = 0;

		if ($this.find("td:nth-child(2) input.act").is(":checked")) {
			enabled = 1;
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
		if (endChannel > 524288) {
			DialogError("Save Channel Outputs",
				"Start Channel '" + startChannel + "' plus Channel Count '" + channelCount + "' exceeds 524288 on row " + rowNumber);
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
		} else if (type == "SPI-WS2801") {
			config = GetSPIOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid SPI-WS2801 Config");
				return;
			}
			maxChannels = 1530;
		} else if (type == "SPI-nRF24L01") {
			config = GetnRFSpeedConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid nRF Config");
				return;
			}
			maxChannels = 512;
		} else if (type == "Triks-C") {
			config = GetTriksOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Triks-C Config");
				return;
			}
			maxChannels = 9216;
		} else if (type == "GPIO") {
			config = GetGPIOOutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid GPIO Config");
				return;
			}
			maxChannels = 1;
		} else if (type == "GPIO-595") {
			config = GetGPIO595OutputConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid GPIO-595 Config");
				return;
			}
			maxChannels = 128;
		} else if (type == "VirtualDisplay") {
			config = GetVirtualDisplayConfig(config, $this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				DialogError("Save Channel Outputs", "Invalid Virtual Display Config");
				return;
			}
			maxChannels = 524288;
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
		}

		outputs.push(config);

		rowNumber++;
	});

	if (dataError)
		return;

	postData.channelOutputs = outputs;

	// Double stringify so JSON in .json file is surrounded by { }
	var postDataStr = "command=setChannelOutputs&file=co-other&data=" + JSON.stringify(JSON.stringify(postData));

	$.post("fppjson.php", postDataStr).success(function(data) {
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
	} else if (type == "VirtualDisplay") {
		config += NewVirtualDisplayConfig();
		row.find("td input.count").val("524288");
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
	if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black")
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
				"<option value='MAX7219Matrix'>MAX7219 Matrix</option>" +
<?
	}
?>
				"<option value='VirtualMatrix'>Virtual Matrix</option>" +
				"<option value='VirtualDisplay'>Virtual Display</option>" +
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
				<div style="overflow: hidden; padding: 10px;">
					<form id="frmOtherOutputs">
						<input name="command" type="hidden" value="saveOtherOutputs" />
						<table>
							<tr><td width = "70 px"><input id="btnSaveOther" class="buttons" type="button" value = "Save" onClick='SaveOtherChannelOutputs();' /></td>
								<td width = "70 px"><input id="btnAddOther" class="buttons" type="button" value = "Add" onClick="AddOtherOutput();"/></td>
								<td width = "40 px">&nbsp;</td>
								<td width = "70 px"><input id="btnDeleteOther" class="disableButtons" type="button" value = "Delete" onClick="DeleteOtherOutput();"></td>
							</tr>
						</table>
						<table id="tblOtherOutputs" class='channelOutputTable'>
							<thead>
								<tr class='tblheader'>
									<td>#</td>
									<td>Active</td>
									<td>Output Type</td>
									<td>Start Ch.</td>
									<td>Channel<br>Count</td>
									<td>Output Config</td>
								</tr>
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
