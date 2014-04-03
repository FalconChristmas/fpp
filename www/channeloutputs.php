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
						}
				}).fail( function() {
					DialogError("Save E1.31 Universes", "Save Failed");
				});
				return false;
			 }
	});
});

<?
function PopulateInterfaces()
{
	global $settings;

	$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo")));
	$ifaceE131 = "";
	if (isset($settings['E131interface'])) {
		$ifaceE131 = $settings['E131interface'];
	}
	error_log("$ifaceE131:" . $ifaceE131);
	$found = 0;
	if ($ifaceE131 == "") {
		echo "<option value=''></option>";
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
// USB Dongles /dev/ttyUSB*
var USBDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if (preg_match("/^ttyUSB[0-9]/", $fileName)) {
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
// Renard Serial Outputs
var RenardDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^ttyS[0-9]/", $fileName)) ||
			(preg_match("/^ttyUSB[0-9]/", $fileName))) {
			echo "RenardDevices['$fileName'] = '$fileName';\n";
		}
	}
?>

var RenardSpeeds = new Array();
RenardSpeeds[ "19200"] =  "19200";
RenardSpeeds[ "38400"] =  "38400";
RenardSpeeds[ "57600"] =  "57600";
RenardSpeeds["115200"] = "115200";
RenardSpeeds["230400"] = "230400";
RenardSpeeds["460800"] = "460800";
RenardSpeeds["921600"] = "921600";
<?

?>

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

function RenardOutputConfig(config) {
	var items = config.split(";");
	var result = "";

	for (var j = 0; j < items.length; j++) {
		var item = items[j].split("=");

		if (item[0] == "device") {
			result += DeviceSelect(RenardDevices, item[1]) + "&nbsp;&nbsp;";
		} else if (item[0] == "renardspeed") {
			result += RenardSpeedSelect(item[1]);
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

	result += "renardspeed=" + value;

	return result;
}

function NewRenardConfig() {
	var result = "";
	result += DeviceSelect(RenardDevices, "") + "&nbsp;&nbsp;";
	result += RenardSpeedSelect("");
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

function GetSPIOutputConfig(cell) {
	$cell = $(cell);
	var value = $cell.find("select.device").val();

	if (value == "")
		return "";

	return "device=" + value;
}

function NewSPIConfig() {
	var result = "";
	result += "Device: " + DeviceSelect(SPIDevices, "");
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

		newRow += "></td>" +
				"<td>" + type + "</td>" +
				"<td><input class='start' type=text size=6 maxlength=6 value='" + output[2] + "'></td>" +
				"<td><input class='count' type=text size=4 maxlength=4 value='" + output[3] + "'></td>" +
				"<td>";

		if ((type == "DMX-Pro") ||
			(type == "DMX-Open") ||
			(type == "Pixelnet-Lynx") ||
			(type == "Pixelnet-Open")) {
			newRow += USBDeviceConfig(output[4]);
		} else if (type == "Renard") {
			newRow += RenardOutputConfig(output[4]);
		} else if (type == "SPI-WS2801") {
			newRow += SPIDeviceConfig(output[4]);
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

		// Start Channel
		var startChannel = $this.find("td:nth-child(4) input").val();
		if ((startChannel == "") || isNaN(startChannel)) {
			DialogError("Save Channel Outputs",
				"Invalid Start Channel '" + startChannel + "' on row " + rowNumber);
			dataError = 1;
			return;
		}
		output += startChannel + ",";

		// Channel Count
		var channelCount = $this.find("td:nth-child(5) input").val();
		if ((channelCount == "") || isNaN(channelCount)) {
			DialogError("Save Channel Outputs",
				"Invalid Channel Count '" + channelCount + "' on row " + rowNumber);
			dataError = 1;
			return;
		}
		output += channelCount + ",";

		// Get output specific options
		var config = "";
		if ((type == "DMX-Pro") ||
			(type == "DMX-Open") ||
			(type == "Pixelnet-Lynx") ||
			(type == "Pixelnet-Open")) {
			config += GetUSBOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				return;
			}
		} else if (type == "Renard") {
			config += GetRenardOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				return;
			}
		} else if (type == "SPI-WS2801") {
			config += GetSPIOutputConfig($this.find("td:nth-child(6)"));
			if (config == "") {
				dataError = 1;
				return;
			}
		}
		output += config;

		postData.Outputs.push(output);

		rowNumber++;
	});

	if (dataError)
		return;

	postData = "command=setChannelOutputs&data=" + JSON.stringify(postData);

	$.post("fppjson.php", postData).success(function(data) {
		PopulateChannelOutputTable(data);
	}).success(function() {
		$.jGrowl("Channel Output Configuration Saved");
	}).fail(function() {
		DialogError("Save Channel Outputs", "Save Failed");
	});
}

function AddOtherTypeOptions(row, type) {
	var config = "";

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
	} else if (type == "SPI-WS2801") {
		config += NewSPIConfig();
		row.find("td input.count").val("510");
	}

	row.find("td input.start").val("1");
	row.find("td input.start").show();
	row.find("td input.count").show();
	row.find("td:nth-child(6)").html(config);
}

function OtherTypeSelected(selectbox) {
	$row = $(selectbox.parentNode.parentNode);

	var type = $(selectbox).val();
	$row.find("td:nth-child(3)").html(type);

	AddOtherTypeOptions($row, type);
}

function AddOtherOutput() {
	if ((Object.keys(USBDevices).length == 0) &&
		(Object.keys(RenardDevices).length == 0) &&
		(Object.keys(SPIDevices).length == 0)) {
		DialogError("Add Output", "No available devices found for new outputs");
		return;
	}

	var currentRows = $("#tblOtherOutputs > tbody > tr").length;

	var newRow = 
		"<tr class='rowUniverseDetails'><td>" + (currentRows + 1) + "</td>" +
			"<td><input class='act' type=checkbox></td>" +
			"<td><select class='type' onChange='OtherTypeSelected(this);'>" +
				"<option value=''>Select a type</option>";

	if (Object.keys(USBDevices).length > 0) {
		newRow +=
			"<option value='DMX-Pro'>DMX-Pro</option>" +
			"<option value='DMX-Open'>DMX-Open</option>" +
			"<option value='Pixelnet-Lynx'>Pixelnet-Lynx</option>" +
			"<option value='Pixelnet-Open'>Pixelnet-Open</option>";
	}

	if (Object.keys(RenardDevices).length > 0) {
		newRow +=
			"<option value='Renard'>Renard</option>";
	}

	if (Object.keys(SPIDevices).length > 0) {
		newRow +=
			"<option value='SPI-WS2801'>SPI-WS2801</option>";
	}

	newRow +=
			"</select></td>" +
			"<td><input class='start' type='text' size=6 maxlength=6 value='' style='display: none;'></td>" +
			"<td><input class='count' type='text' size=4 maxlength=4 value='' style='display: none;'></td>" +
			"<td>&nbsp;</td>" +
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

<title>Falcon PI Player - FPP</title>
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
	<b>Enable E1.31 Output:</b> <? PrintSettingCheckbox("E1.31 Output", "E131Enabled", "1", "0"); ?><br><br>
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
								<b>Enable FPD Output:</b> <? PrintSettingCheckbox("FPD Output", "FPDEnabled", "1", "0"); ?><br><br>
								<form id="frmPixelnetDMX">
									<input name="command" type="hidden" value="savePixelnetDMX" />
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
	<tr class='tblheader'><td>#</td><td>Act</td><td>Type</td><td>Start</td><td>Size</td><td>Output Config</td></tr>
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
