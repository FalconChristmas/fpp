<script>
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


<?
$LEDPanelOutputs = 12;
$LEDPanelPanelsPerOutput = 16;
$LEDPanelRows = 1;
$LEDPanelCols = 1;
$LEDPanelWidth = 32;
$LEDPanelHeight = 16;
$LEDPanelScan = 8;
$LEDPanelInterleave = 0;

if ($settings['Platform'] == "BeagleBone Black")
{
    $LEDPanelPanelsPerOutput = 16;
    if (strpos($settings['SubPlatform'], 'Green Wireless') !== FALSE) {
        $LEDPanelOutputs = 5;
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE) {
        $LEDPanelOutputs = 6;
    } else {
        $LEDPanelOutputs = 8;
    }
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

function printLEDPanelSizeSelect($platform, $def)
{
	$values = array();
    if ($platform == "BeagleBone Black") {
        $values["32x16 1/8 Scan"] = "32x16x8";
        $values["32x16 1/4 Scan"] = "32x16x4";
        $values["32x16 1/2 Scan"] = "32x16x2";
        $values["32x32 1/16 Scan"] = "32x32x16";
        $values["64x32 1/16 Scan"] = "64x32x16";
        
        $values["64x32 1/8 Scan"] = "64x32x8";
        $values["32x32 1/8 Scan"] = "32x32x8";
        $values["40x20 1/5 Scan"] = "40x20x5";
    } else {
        $values["32x16"] = "32x16x8";
        $values["32x32"] = "32x32x16";
    }
    PrintSettingSelect("Panel Size", "LEDPanelsSize", 1, 0, $def, $values, "", "LEDPanelLayoutChanged");
}

function printLEDPanelInterleaveSelect($platform, $interleave)
{
    $values = array();

    $values["Off"] = "0";
    $values["8 Pixels"] = "8";
    $values["16 Pixels"] = "16";
    $values["32 Pixels"] = "32";
    $values["64 Pixels"] = "64";
    PrintSettingSelect("Panel Interleave", "LEDPanelInterleave", 1, 0, $interleave, $values, "", "LEDPanelLayoutChanged");
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
    LEDPanelInterleave = parseInt($('#LEDPanelInterleave').val());
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

function GetLEDPanelColorOrder(key, selectedItem)
{
	var colorOrders = [ "RGB", "RBG", "GBR", "GRB", "BGR", "BRG" ];
	var html = "";
	var selected = "";
	var i = 0;

	html += "<select id='" + key + "'>";
	html += "<option value=''>C-Default</option>";
	for (i = 0; i < colorOrders.length; i++)
	{
		selected = "";

		if (colorOrders[i] == selectedItem)
			selected = "selected";

		html += "<option value='" + colorOrders[i] + "' " + selected + ">C-" + colorOrders[i] + "</option>";
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

	UpdatePanelSize();
    if ((LEDPanelScan * 2) == LEDPanelHeight) {
        $('#LEDPanelInterleave').attr("disabled", "disabled");
        LEDPanelsInterleave = 0;
    } else {
        $('#LEDPanelInterleave').removeAttr("disabled");
    }

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
			html += "<br>";

			key = "LEDPanelColorOrder_" + r + "_" + c;
			if (typeof channelOutputsLookup[key] !== 'undefined') {
				html += GetLEDPanelColorOrder(key, channelOutputsLookup[key].colorOrder);
			} else {
				html += GetLEDPanelColorOrder(key, "");
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

	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
		$('#LEDPanelsGPIOSlowdown').val(channelOutputsLookup["LEDPanelMatrix"].gpioSlowdown);
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
			LEDPanelPanelsPerOutput = 16;
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

	if ($settings['Platform'] == "Raspberry Pi")
	{
?>
		config.gpioSlowdown = parseInt($('#LEDPanelsGPIOSlowdown').val());
<?
	}

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

			id = "#LEDPanelColorOrder_" + r + "_" + c;
			panel.colorOrder = $(id).val();

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
		$('#LEDPanelsGPIOSlowdownLabel').hide();
		$('#LEDPanelsGPIOSlowdown').hide();
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
if ($settings['Platform'] == "BeagleBone Black") {
    if (strpos($settings['SubPlatform'], 'Green Wireless') !== FALSE) {
        echo "        LEDPanelOutputs = 5;\n";
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE) {
        echo "        LEDPanelOutputs = 6;\n";
    } else {
        echo "        LEDPanelOutputs = 8;\n";
    }
} else {
?>
		LEDPanelOutputs = 3;
		$('#LEDPanelsGPIOSlowdownLabel').show();
		$('#LEDPanelsGPIOSlowdown').show();
<?
}
?>
	}

	DrawLEDPanelTable();
}

$(document).ready(function(){
	InitializeLEDPanels();
	LEDPannelsConnectionChanged();
});

</script>

<div id='tab-LEDPanels'>
	<div id='divLEDPanels'>
		<fieldset class="fs">
			<legend> LED Panels </legend>
			<div id='divLEDPanelsData'>
				<div style="overflow: hidden; padding: 10px;">
					<table border=0 cellspacing=3>
						<tr><td><b>Enable LED Panel Output:</b></td>
							<td><input id='LEDPanelsEnabled' type='checkbox'></td>
							<td width=20>&nbsp;</td>
							<td>&nbsp;</td>
						</tr>
						<tr><td><b>Panel Layout (WxH):</b></td>
							<td><? printLEDPanelLayoutSelect(); ?></td>
							<td>&nbsp;</td>
							<td><b>Start Channel:</b></td>
							<td><input id='LEDPanelsStartChannel' type=text size=6 maxlength=6 value='1'></td>
						</tr>
						<tr><td><b>Single Panel Size (WxH):</b></td>
							<td><? printLEDPanelSizeSelect($settings['Platform'], $LEDPanelWidth + "x" + $LEDPanelHeight + "x" + $LEDPanelScan); ?></td>
							<td>&nbsp;</td>
							<td><b>Channel Count:</b></td>
							<td><span id='LEDPanelsChannelCount'>1536</span></td>
						</tr>
						<tr><td><b>Model Start Corner:</b></td>
							<td><select id='LEDPanelsStartCorner'>
									<option value='0'>Top Left</option>
									<option value='1'>Bottom Left</option>
								</select>
							</td>
							<td>&nbsp;</td>
							<td><b>Default Panel Color Order:</b></td>
							<td><select id='LEDPanelsColorOrder'>
									<option value='RGB'>RGB</option>
									<option value='RBG'>RBG</option>
									<option value='GRB'>GRB</option>
									<option value='GBR'>GBR</option>
									<option value='BRG'>BRG</option>
									<option value='BGR'>BGR</option>
								</select>
							</td>
						</tr>
						<tr><td><b>Brightness:</b></td>
							<td><select id='LEDPanelsBrightness'>
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
							<td><b>Wiring Pinout:</b></td>
							<td><select id='LEDPanelsWiringPinout'>
<?
if ($settings['Platform'] == "Raspberry Pi")
{
?>
									<option value='regular'>Standard</option>
									<option value='adafruit-hat'>Adafruit</option>
									<option value='adafruit-hat-pwm'>Adafruit PWM</option>
									<option value='regular-pi1'>Standard Pi1</option>
									<option value='classic'>Classic</option>
									<option value='classic-pi1'>Classic Pi1</option>
<?
}
else if ($settings['Platform'] == "BeagleBone Black")
{
	if (strpos($settings['SubPlatform'], 'Green Wireless') !== FALSE)
	{
?>
									<option value='v2'>v2.x</option>
<?
	}
	else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE)
	{
?>
									<option value='PocketScroller1x'>PocketScroller</option>
<?
	}
	else
	{
?>
									<option value='v1'>Standard v1.x</option>
									<option value='v2'>v2.x</option>
<?
	}
}
?>
								</select>
							</td>
						</tr>
<?
if ($settings['Platform'] == "BeagleBone Black") {
?>
                        <tr><td><b>Panel Interleave:</b></td>
							<td><? printLEDPanelInterleaveSelect($settings['Platform'], $LEDPanelInterleave); ?></td>
							</td>
						</tr>
						<tr><td><b>Color Depth:</b></td>
							<td><select id='LEDPanelsColorDepth'>
									<option value='8'>8 Bit</option>
									<option value='7'>7 Bit</option>
									<option value='6'>6 Bit</option>
								</select>
							</td>
						</tr>
<?
}
?>
						<tr><td><b>Connection:</b></td>
							<td><select id='LEDPanelsConnection' onChange='LEDPannelsConnectionChanged();'>
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
							</td>
							<td>&nbsp;</td>
							<td><span id='LEDPanelsConnectionInterface'><b>Interface:</b></span>
								<span id='LEDPanelsGPIOSlowdownLabel'><b>GPIO Slowdown:</b></span>
								</td>
							<td><select id='LEDPanelsInterface' type='hidden'>
<? PopulateEthernetInterfaces(); ?>
								</select>
								<select id='LEDPanelsGPIOSlowdown'>
									<option value='0'>0 (Pi Zero and other single-core)</option>
									<option value='1' selected>1 (multi-core Pi's)</option>
									<option value='2'>2 (slow panels)</option>
									<option value='3'>3 (slower panels)</option>
								</select>
							</td>
						</tr>

						<tr id='LEDPanelsSourceMac'>
							<td><b>Source Mac:</b></td>
							<td><input id='LEDPanelsSourceMacInput' type=text size=16 maxlength=17 value='00:00:00:00:00:00'></td>
								<td>&nbsp;</td>
						</tr>
						<tr><td width = '70 px' colspan=5><input id='btnSaveChannelOutputsJSON' class='buttons' type='button' value='Save' onClick='SaveChannelOutputsJSON();'/> <font size=-1><? if ($settings['Platform'] == "BeagleBone Black") { echo "(this will save changes to BBB tab &amp; LED Panels tab)"; } ?></font></td>
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
					- C-(color) is color order if panel has different color order than default.<br>
					- Arrow <img src='images/arrow_N.png' height=17 width=17> indicates panel orientation, click arrow to rotate.<br>
				</div>
			</div>
		</fieldset>
	</div>
</div>
