<script src='js/fabric.min.js'></script>
<script>

<?
function readPanelCapes($cd, $panelCapes)
{
    if (is_dir($cd)) {
        if ($dh = opendir($cd)) {
            while (($file = readdir($dh)) !== false) {
                $string = "";
                if (substr($file, 0, 1) == '.') {
                    $string = "";
                } else {
                    $string = file_get_contents($cd . $file);
                }
                if ($string != "") {
                    $panelCapes[] = $string;
                }
            }
            closedir($dh);
        }
    }
    return $panelCapes;
}

$panelCapes = array();
$panelCapes = readPanelCapes($mediaDirectory . "/tmp/panels/", $panelCapes);
$panelCapesHaveSel4 = false;
if (count($panelCapes) == 1) {
    echo "var KNOWN_PANEL_CAPE = " . $panelCapes[0] . ";";
    $panelCapes[0] = json_decode($panelCapes[0], true);
    if (isset($panelCapes[0]["controls"]["sel4"])) {
        $panelCapesHaveSel4 = true;
    }
} else {
    echo "// NO KNOWN_PANEL_CAPE";
}

$LEDPanelOutputs = 24;
$LEDPanelPanelsPerOutput = 24;
$LEDPanelRows = 1;
$LEDPanelCols = 1;
$LEDPanelWidth = 32;
$LEDPanelHeight = 16;
$LEDPanelScan = 8;
$LEDPanelAddressing = 0;
$LEDPanelGamma = 2.2;

if ($settings['Platform'] == "BeagleBone Black") {
    $LEDPanelPanelsPerOutput = 16;
    if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) {
        $LEDPanelOutputs = 5;
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
        $LEDPanelOutputs = 6;
    } else {
        $LEDPanelOutputs = 8;
    }
}

$maxLEDPanels = $LEDPanelOutputs * $LEDPanelPanelsPerOutput;
$maxLEDPanels = 96; // Override to allow different panel configs using ColorLight cards

if (isset($settings['LEDPanelsLayout'])) {
    $parts = explode('x', $settings['LEDPanelsLayout']);
    if (count($parts) == 2) {
        $LEDPanelCols = $parts[0];
        $LEDPanelRows = $parts[1];
    }
}

function printLEDPanelLayoutSelect()
{
    global $maxLEDPanels, $LEDPanelCols, $LEDPanelRows;

    echo "W: <select id='LEDPanelsLayoutCols' onChange='LEDPanelsLayoutChanged()'>\n";
    for ($r = 1; $r <= $maxLEDPanels; $r++) {
        if ($LEDPanelCols == $r) {
            echo "<option value='" . $r . "' selected>" . $r . "</option>\n";
        } else {
            echo "<option value='" . $r . "'>" . $r . "</option>\n";
        }
    }
    echo "</select>";

    echo "&nbsp; H:<select id='LEDPanelsLayoutRows' onChange='LEDPanelsLayoutChanged()'>\n";
    for ($r = 1; $r <= $maxLEDPanels; $r++) {
        if ($LEDPanelRows == $r) {
            echo "<option value='" . $r . "' selected>" . $r . "</option>\n";
        } else {
            echo "<option value='" . $r . "'>" . $r . "</option>\n";
        }
    }
    echo "</select>";
}

function printLEDPanelGammaSelect($platform, $gamma)
{
    if (!isset($gamma) || $gamma == "" || $gamma == "0") {
        $gamma = "2.2";
    }
    echo "<input type='number' min='0.1' max='5.0' step='0.1' value='$gamma' id='LEDPanelsGamma'>";
}

function printLEDPanelInterleaveSelect($platform)
{
    echo "<select id='LEDPanelInterleave' onchange='LEDPanelLayoutChanged();'>";

    $values = array();
    if ($platform == "BeagleBone Black") {
        $values["Off"] = "0";
        $values["4 Pixels"] = "4";
        $values["8 Pixels"] = "8";
        $values["16 Pixels"] = "16";
        $values["32 Pixels"] = "32";
        $values["64 Pixels"] = "64";
        $values["80 Pixels"] = "80";
        $values["4 Pixels Zig/Zag"] = "4z";
        $values["8 Pixels Zig/Zag"] = "8z";
        $values["16 Pixels Zig/Zag"] = "16z";
        $values["8 Pixels Flip Rows"] = "8f";
        $values["16 Pixels Flip Rows"] = "16f";
        $values["32 Pixels Flip Rows"] = "32f";
        $values["64 Pixels Flip Rows"] = "64f";
        $values["80 Pixels Flip Rows"] = "80f";
        $values["8 Pixels Cluster Zig/Zag"] = "8c";
        $values["8 Stripe/16 Cluster"] = "8s";
    } else {
        $values["Off"] = "0";
        $values["Stripe"] = "1";
        $values["Checkered"] = "2";
        $values["Spiral"] = "3";
        $values["ZStripe"] = "4";
        $values["ZnMirrorZStripe"] = "5";
        $values["Coreman"] = "6";
        $values["Kaler2Scan"] = "7";
        $values["ZStripeUneven"] = "8";
        $values["P10-128x4-Z"] = "9";
        $values["QiangLiQ8"] = "10";
    }
    foreach ($values as $key => $value) {
        echo "<option value='$value'";
        if ($value == "0") {
            echo " selected";
        }

        echo ">$key</option>\n";
    }
    echo "</select>";
}

?>

var LEDPanelColorOrder = 'RGB';
var LEDPanelOutputs = <?echo $LEDPanelOutputs; ?>;
var LEDPanelPanelsPerOutput = <?echo $LEDPanelPanelsPerOutput; ?>;
var LEDPanelWidth = <?echo $LEDPanelWidth; ?>;
var LEDPanelHeight = <?echo $LEDPanelHeight; ?>;
var LEDPanelScan = <?echo $LEDPanelScan; ?>;
var LEDPanelAddressing = <?echo $LEDPanelAddressing; ?>;
var LEDPanelRows = <?echo $LEDPanelRows; ?>;
var LEDPanelCols = <?echo $LEDPanelCols; ?>;
var LEDPanelGamma = <?echo $LEDPanelGamma; ?>;

function UpdatePanelSize()
{
	var size = $('#LEDPanelsSize').val();
	var sizeparts = size.split("x");
	LEDPanelWidth = parseInt(sizeparts[0]);
	LEDPanelHeight = parseInt(sizeparts[1]);
    LEDPanelScan = parseInt(sizeparts[2]);
    LEDPanelAddressing = parseInt(sizeparts[3]);
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
	html += "<option value=''>C-Def</option>";
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

function UpdateLegacyLEDPanelLayout()
{
	LEDPanelCols = parseInt($('#LEDPanelsLayoutCols').val());
	LEDPanelRows = parseInt($('#LEDPanelsLayoutRows').val());

	UpdatePanelSize();
    if ((LEDPanelScan * 2) == LEDPanelHeight) {
        $('#LEDPanelInterleave').attr("disabled", "disabled");
    } else {
        $('#LEDPanelInterleave').removeAttr("disabled");
    }

    var pixelCount = LEDPanelCols * LEDPanelRows * LEDPanelWidth * LEDPanelHeight;
    $('#LEDPanelsPixelCount').html(pixelCount);

    var channelCount = pixelCount * 3;
    $('#LEDPanelsChannelCount').html(channelCount);

	DrawLEDPanelTable();
}

function LEDPanelLayoutChanged()
{
    UpdateLegacyLEDPanelLayout();

    // update in-memory array
    GetChannelOutputConfig();

    if ($('#LEDPanelUIAdvancedLayout').is(":checked")) {
        for (var i = 0; i < channelOutputsLookup["LEDPanelMatrix"].panels.length; i++) {
            SetupCanvasPanel(i);
        }
        UpdateMatrixSize();
    }
}


function LEDPanelsLayoutChanged() {
    var value = $('#LEDPanelsLayoutCols').val() + "x" + $('#LEDPanelsLayoutRows').val();

    $.put('api/settings/LEDPanelsLayout', value)
        .done(function() {
            $.jGrowl('Panel Layout saved',{themeState:'success'});
            settings['LEDPanelsLayout'] = value;
            LEDPanelLayoutChanged();
            SetRestartFlag(2);
            common_ViewPortChange();
        }).fail(function() {
            DialogError('Panel Layout', 'Failed to save Panel Layout');
        });
}


function FrontBackViewToggled() {
    GetChannelOutputConfig(); // Refresh the in-memory config before redrawing
    DrawLEDPanelTable();
}

function DrawLEDPanelTable()
{
	var r;
	var c;
	var key = "";
	var frontView = 0;

    var tbody = $('#LEDPanelTable tbody');
    tbody.empty();
	if ($('#LEDPanelUIFrontView').is(":checked")) {
		frontView = 1;
        tbody[0].insertRow(0).innerHTML = "<th colspan='" + LEDPanelCols + "'>Front View</th>";
	} else {
        tbody[0].insertRow(0).innerHTML = "<th colspan='" + LEDPanelCols + "'>Back View</th>";
	}

	for (r = 0 ; r < LEDPanelRows; r++) {
		var html = "<tr>";
		for (i = 0; i < LEDPanelCols; i++) {
			if (frontView)
				c = i;
			else
				c = LEDPanelCols - 1 - i;

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
        tbody[0].insertRow(-1).innerHTML = html;
	}
}

function InitializeLEDPanels()
{
	if (("LEDPanelMatrix-Enabled" in channelOutputsLookup) &&
			(channelOutputsLookup["LEDPanelMatrix-Enabled"])) {
		$('#LEDPanelsEnabled').prop('checked', true);
        $('#tab-LEDPanels-LI').show();
    }

	if ("LEDPanelMatrix" in channelOutputsLookup)
	{
		$('#LEDPanelsStartChannel').val(channelOutputsLookup["LEDPanelMatrix"].startChannel);
		$('#LEDPanelsPixelCount').html(channelOutputsLookup["LEDPanelMatrix"].channelCount / 3);
		$('#LEDPanelsChannelCount').html(channelOutputsLookup["LEDPanelMatrix"].channelCount);
		$('#LEDPanelsColorOrder').val(channelOutputsLookup["LEDPanelMatrix"].colorOrder);
		$('#LEDPanelsBrightness').val(channelOutputsLookup["LEDPanelMatrix"].brightness);
        $('#LEDPanelsGamma').val(channelOutputsLookup["LEDPanelMatrix"].gamma);
		$('#LEDPanelsConnection').val(channelOutputsLookup["LEDPanelMatrix"].subType);
        if (channelOutputsLookup["LEDPanelMatrix"].interface != null) {
            $('#LEDPanelsInterface').val(channelOutputsLookup["LEDPanelMatrix"].interface);
        }
<?
if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black") {
    ?>
		$('#LEDPanelsWiringPinout').val(channelOutputsLookup["LEDPanelMatrix"].wiringPinout);
<?
}

if ($settings['Platform'] == "Raspberry Pi") {
    ?>
		$('#LEDPanelsGPIOSlowdown').val(channelOutputsLookup["LEDPanelMatrix"].gpioSlowdown);
<?
}
?>
        var outputByRow = false;
        var outputBlank = false;
        if (channelOutputsLookup["LEDPanelMatrix"].panelOutputOrder != null) {
            outputByRow = channelOutputsLookup["LEDPanelMatrix"].panelOutputOrder;
        }
        var colordepth = channelOutputsLookup["LEDPanelMatrix"].panelColorDepth;
        if (typeof colordepth === 'undefined') {
            colordepth = 8;
        }
        if (channelOutputsLookup["LEDPanelMatrix"].panelRowAddressType != null) {
            var RowAddressType = channelOutputsLookup["LEDPanelMatrix"].panelRowAddressType;
            $('#LEDPanelRowAddressType').val(RowAddressType);
        }
        if (channelOutputsLookup["LEDPanelMatrix"].panelInterleave != null) {
            var interleave = channelOutputsLookup["LEDPanelMatrix"].panelInterleave;
            $('#LEDPanelInterleave').val(interleave);
        }

<?if ($settings['Platform'] == "BeagleBone Black") {?>
        if (channelOutputsLookup["LEDPanelMatrix"].panelOutputBlankRow != null) {
            outputBlank = channelOutputsLookup["LEDPanelMatrix"].panelOutputBlankRow;
        }
        if (colordepth < 0) {
            outputBlank = true;
            colordepth = -colordepth;
            outputByRow = true;
        }
        $('#LEDPanelsOutputByRow').prop("checked", outputByRow);
        $('#LEDPanelsOutputBlankRow').prop("checked", outputBlank);
        if (outputByRow == false) {
            $('#LEDPanelsOutputBlankRow').hide();
            $('#LEDPanelsOutputBlankLabel').hide();
        }
<?} else if ($settings['Platform'] == "Raspberry Pi") {?>
        var cpuPWM = false;
        var cpuPWM = channelOutputsLookup["LEDPanelMatrix"].cpuPWM;
        if (typeof cpuPWM === 'undefined') {
            cpuPWM = false;
        }
        $('#LEDPanelsOutputCPUPWM').prop("checked", cpuPWM);
<?}?>

        $('#LEDPanelsColorDepth').val(colordepth);
		$('#LEDPanelsStartCorner').val(channelOutputsLookup["LEDPanelMatrix"].invertedData);

		if ((channelOutputsLookup["LEDPanelMatrix"].subType == 'ColorLight5a75') ||
            (channelOutputsLookup["LEDPanelMatrix"].subType == 'X11PanelMatrix'))
		{
			LEDPanelOutputs = 24;
			LEDPanelPanelsPerOutput = 24;
		}
    }

    <?
if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black") {
    ?>

    if (typeof KNOWN_PANEL_CAPE  !== 'undefined') {
        if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"]  !== 'undefined') {
            $('#LEDPanelsWiringPinout').val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"]);
            $('#LEDPanelsWiringPinout').hide();
            $('#LEDPanelsWiringPinoutLabel').hide();
        }
        if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"]  !== 'undefined') {
            $('#LEDPanelsConnection').val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"]);
            $('#LEDPanelsConnection').hide();
            $('#LEDPanelsConnectionLabel').hide();
        }
        LEDPanelOutputs = KNOWN_PANEL_CAPE["outputs"].length;
    }
    <?
}
?>

    PanelSubtypeChanged();
    UpdateLegacyLEDPanelLayout();
}

function GetLEDPanelConfig()
{
	var config = new Object();
	var panels = "";
	var xOffset = 0;
	var yOffset = 0;
	var yDiff = 0;
    var advanced = 0;

    if (($('#LEDPanelUIAdvancedLayout').is(":checked")) &&
        (typeof channelOutputsLookup["LEDPanelMatrix"] !== 'undefined'))
        advanced = 1;

	config.type = "LEDPanelMatrix";
<?
if ($settings['Platform'] == "BeagleBone Black") {
    echo "config.subType = 'LEDscapeMatrix';\n";
} else {
    echo "config.subType = 'RGBMatrix';\n";
}
?>

	UpdatePanelSize();

	config.enabled = 0;
	config.cfgVersion = 2;
	config.startChannel = parseInt($('#LEDPanelsStartChannel').val());
   	config.channelCount = parseInt($('#LEDPanelsChannelCount').html());
	config.colorOrder = $('#LEDPanelsColorOrder').val();
    config.gamma = $('#LEDPanelsGamma').val();
	if (($('#LEDPanelsConnection').val() === "ColorLight5a75") || ($('#LEDPanelsConnection').val() === "X11PanelMatrix"))
	{
		config.subType = $('#LEDPanelsConnection').val();
        if ($('#LEDPanelsConnection').val() != "X11PanelMatrix")
            config.interface = $('#LEDPanelsInterface').val();

	}
<?
if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black") {
    ?>
	config.wiringPinout = $('#LEDPanelsWiringPinout').val();
    config.brightness = parseInt($('#LEDPanelsBrightness').val());
<?
}

if ($settings['Platform'] == "Raspberry Pi") {
    ?>
		config.gpioSlowdown = parseInt($('#LEDPanelsGPIOSlowdown').val());
<?
}
?>

    config.panelColorDepth = parseInt($('#LEDPanelsColorDepth').val());
    config.gamma = $('#LEDPanelsGamma').val();
	config.invertedData = parseInt($('#LEDPanelsStartCorner').val());
    config.ledPanelsLayout = $('#LEDPanelsLayoutCols').val() + "x" + $('#LEDPanelsLayoutRows').val();
    config.panelWidth = LEDPanelWidth;
	config.panelHeight = LEDPanelHeight;
    config.panelScan = LEDPanelScan;
    <?if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black") {?>
        config.panelOutputOrder = $('#LEDPanelsOutputByRow').is(':checked');
        config.panelOutputBlankRow = $('#LEDPanelsOutputBlankRow').is(':checked');
    <?}?>
    <?if ($settings['Platform'] == "Raspberry Pi") {?>
        config.cpuPWM = $('#LEDPanelsOutputCPUPWM').is(':checked');
    <?}?>

    if (LEDPanelAddressing) {
        config.panelAddressing = LEDPanelAddressing;
    }
    if ($('#LEDPanelRowAddressType').val() != "0") {
        config.panelRowAddressType = parseInt($('#LEDPanelRowAddressType').val());
    }
    if ($('#LEDPanelInterleave').val() != "0") {
        config.panelInterleave = $('#LEDPanelInterleave').val();
    }
	config.panels = [];

	if ($('#LEDPanelsEnabled').is(":checked"))
		config.enabled = 1;

    if (advanced) {
		config.panels = GetAdvancedPanelConfig();
    } else {
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
	}

	return config;
}


<?
function PopulateEthernetInterfaces()
{
    $interfaces = network_list_interfaces_array();
    foreach ($interfaces as $iface) {
        $iface = preg_replace("/:$/", "", $iface);
        echo "<option value='" . $iface;
        echo "'";
        if ($iface === "eth0") {
            echo " selected";
        }
        
        $ifaceSpeed = (int)exec("$SUDO ethtool $iface | grep -i 'baset' | grep -Eo '[0-9]{1,4}' | sort | tail -1");
        echo ">" . $iface . " (" . $ifaceSpeed ."Mbps)</option>";
    }
}
?>

function LEDPanelsConnectionChanged()
{
    WarnIfSlowNIC();

	if (($('#LEDPanelsConnection').val() === "ColorLight5a75") || ($('#LEDPanelsConnection').val() === "X11PanelMatrix")) {
		$('#LEDPanelsGPIOSlowdownLabel').hide();
		$('#LEDPanelsGPIOSlowdown').hide();
        $('#LEDPanelsBrightness').hide();
        $('#LEDPanelsColorDepth').hide();
        $('#LEDPanelsBrightnessLabel').hide();
        $('#LEDPanelsColorDepthLabel').hide();
        $('#LEDPanelsWiringPinoutLabel').hide();
        $('#LEDPanelsWiringPinout').hide();

        if ($('#LEDPanelsConnection').val() === "X11PanelMatrix") {
            $('#LEDPanelsConnectionInterface').hide();
            $('#LEDPanelsInterface').hide();
        } else {
            $('#LEDPanelsConnectionInterface').show();
            $('#LEDPanelsInterface').show();
        }

<?
if ($settings['Platform'] == "BeagleBone Black") {
    echo "        $('#LEDPanelsRowAddressTypeLabel').hide();\n";
    echo "        $('#LEDPanelRowAddressType').hide();\n";
    echo "        $('#LEDPanelsInterleaveLabel').hide();\n";
    echo "        $('#LEDPanelInterleave').hide();\n";
    echo "        $('#LEDPanelsOutputByRowLabel').hide();\n";
    echo "        $('#LEDPanelsOutputByRow').hide();\n";
    echo "        $('#LEDPanelsOutputBlankRowLabel').hide();\n";
    echo "        $('#LEDPanelsOutputBlankRow').hide();\n";
}
if ($settings['Platform'] == "Raspberry Pi") {
    echo "        $('#LEDPanelsInterleaveLabel').hide();\n";
    echo "        $('#LEDPanelInterleave').hide();\n";
    echo "        $('#LEDPanelsOutputCPUPWMLabel').hide();\n";
    echo "        $('#LEDPanelsOutputCPUPWM').hide();\n";
}
?>

		LEDPanelOutputs = 24;
	}
	else
	{
		$('#LEDPanelsConnectionInterface').hide();
		$('#LEDPanelsInterface').hide();
        $('#LEDPanelsBrightness').show();
        $('#LEDPanelsColorDepth').show();
        $('#LEDPanelsBrightnessLabel').show();
        $('#LEDPanelsColorDepthLabel').show();
        $('#LEDPanelsWiringPinoutLabel').show();
        $('#LEDPanelsWiringPinout').show();

<?
if ($settings['Platform'] == "BeagleBone Black") {
    echo "        $('#LEDPanelsInterleaveLabel').show();\n";
    echo "        $('#LEDPanelInterleave').show();\n";
    echo "        $('#LEDPanelsOutputByRowLabel').show();\n";
    echo "        $('#LEDPanelsOutputByRow').show();\n";
    echo "        var checked = $('#LEDPanelsOutputByRow').is(':checked')\n";
    echo "        if (checked != false) {\n";
    echo "            $('#LEDPanelsOutputBlankRowLabel').show();\n";
    echo "            $('#LEDPanelsOutputBlankRow').show();\n";
    echo "        } else {\n";
    echo "            $('#LEDPanelsOutputBlankRowLabel').hide();\n";
    echo "            $('#LEDPanelsOutputBlankRow').hide();\n";
    echo "        }\n";

    if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) {
        echo "        LEDPanelOutputs = 5;\n";
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
        echo "        LEDPanelOutputs = 6;\n";
    } else {
        echo "        LEDPanelOutputs = 8;\n";
    }
    ?>
    $('#LEDPanelsGPIOSlowdownLabel').hide();
    $('#LEDPanelsGPIOSlowdown').hide();
<?
} else {
    if ($settings['Platform'] == "Raspberry Pi") {
        echo "        $('#LEDPanelsRowAddressTypeLabel').show();\n";
        echo "        $('#LEDPanelRowAddressType').show();\n";
        echo "        $('#LEDPanelsInterleaveLabel').show();\n";
        echo "        $('#LEDPanelInterleave').show();\n";
        echo "        $('#LEDPanelsOutputCPUPWMLabel').show();\n";
        echo "        $('#LEDPanelsOutputCPUPWM').show();\n";
    }
    ?>
		LEDPanelOutputs = 3;
		$('#LEDPanelsGPIOSlowdownLabel').show();
		$('#LEDPanelsGPIOSlowdown').show();
<?
}
?>
	}

    if (typeof KNOWN_PANEL_CAPE  !== 'undefined') {
        if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"]  !== 'undefined') {
            $('#LEDPanelsWiringPinout').val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"]);
            $('#LEDPanelsWiringPinout').hide();
            $('#LEDPanelsWiringPinoutLabel').hide();
        }
        if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"]  !== 'undefined') {
            $('#LEDPanelsConnection').val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"]);
            $('#LEDPanelsConnection').hide();
            $('#LEDPanelsConnectionLabel').hide();
        }
        LEDPanelOutputs = KNOWN_PANEL_CAPE["outputs"].length;
    }

    PanelSubtypeChanged();
	DrawLEDPanelTable();
}

<?if ($settings['Platform'] == "BeagleBone Black") {
    echo "function outputByRowClicked() {\n";
    echo "        var checked = $('#LEDPanelsOutputByRow').is(':checked')\n";
    echo "        if (checked != false) {\n";
    echo "            $('#LEDPanelsOutputBlankRowLabel').show();\n";
    echo "            $('#LEDPanelsOutputBlankRow').show();\n";
    echo "        } else {\n";
    echo "            $('#LEDPanelsOutputBlankRowLabel').hide();\n";
    echo "            $('#LEDPanelsOutputBlankRow').hide();\n";
    echo "        }\n";
    echo "    }\n";
}
?>

/////////////////////////////////
var canvasInitialized = 0;
var uiScale = 3;
var panelGroups = [];
var ledPanelCanvas;
var selectedPanel = -1;
var canvasWidth = 900;
var canvasHeight = 400;

function GetAdvancedPanelConfig() {
    var co = channelOutputsLookup["LEDPanelMatrix"];
    var panels = [];

    for (var key in panelGroups) {
        var panel = new Object();
        var pg = panelGroups[key];

        p = co.panels[pg.panelNumber];

        panel.outputNumber = p.outputNumber;
        panel.panelNumber = p.panelNumber;
        panel.xOffset = Math.round(pg.group.left / uiScale);
        panel.yOffset = Math.round(pg.group.top / uiScale);
        panel.orientation = p.orientation;
        panel.colorOrder = p.colorOrder;
        panel.row = p.row;
        panel.col = p.col;

        panels.push(panel);
    }

    return panels;
}

function snapToGrid(o) {
    var co = channelOutputsLookup["LEDPanelMatrix"];
    var left = Math.round(o.left / uiScale);
    var top  = Math.round(o.top / uiScale);
    var maxLeft = (canvasWidth / uiScale) - co.panelWidth;
    var maxTop = (canvasHeight / uiScale) - co.panelHeight;

    if ((o.orientation == 'R') ||
        (o.orientation == 'L')) {
        maxLeft = (canvasWidth / uiScale) - co.panelHeight;
        maxTop = (canvasHeight / uiScale) - co.panelWidth;
    }

    if (left < 0)
        left = 0;
    else if (left > maxLeft)
        left = maxLeft;

    if (top < 0)
        top = 0;
    else if (top > maxTop)
        top = maxTop;

    o.set({
        left: left * uiScale,
        top: top * uiScale
    });
}

function panelMovingHandler(evt) {
    var o = evt.target;
    var panels = channelOutputsLookup["LEDPanelMatrix"].panels;
    var co = channelOutputsLookup["LEDPanelMatrix"];
    var name = o.name;
    var outputNumber = o.outputNumber;
    var panelNumber = o.panelNumber;
    var colorOrder = o.colorOrder;

    snapToGrid(o);

    var left = Math.round(o.left / uiScale);
    var top  = Math.round(o.top / uiScale);
    var desc = 'O-' + (outputNumber+1) + ' P-' + (panelNumber+1) + '\n@ ' + left + ',' + top;
    panelGroups[name].text.set('text', desc);

    channelOutputsLookup["LEDPanelMatrix"].panels[panelGroups[name].panelNumber].xOffset = left;
    channelOutputsLookup["LEDPanelMatrix"].panels[panelGroups[name].panelNumber].yOffset = top;

    panelSelectedHandler(evt);
    ledPanelCanvas.renderAll();

    UpdateMatrixSize();
}

function panelSelectedHandler(evt) {
    var o = ledPanelCanvas.getActiveObject();

    if (selectedPanel != -1)
        panelGroups['panel-' + selectedPanel].panel.set({'stroke':'black'});

    if (!o) {
        $('#cpOutputNumber').val(0);
        $('#cpPanelNumber').val(0);
        $('#cpColorOrder').val('');
        $('#cpXOffset').html('');
        $('#cpYOffset').html('');
        selectedPanel = -1;
    } else {
        $('#cpOutputNumber').val(o.outputNumber);
        $('#cpPanelNumber').val(o.panelNumber);
        $('#cpColorOrder').val(o.colorOrder);
        $('#cpXOffset').html(Math.round(o.left / uiScale));
        $('#cpYOffset').html(Math.round(o.top / uiScale));
        selectedPanel = panelGroups[o.name].panelNumber;
        panelGroups[o.name].panel.set({'stroke':'rgb(255,255,0)'});
    }

    ledPanelCanvas.renderAll();
}

function RotateCanvasPanel() {
    var selection = ledPanelCanvas.getActiveObject();

    if (!selection) {
        return;
    } else if (selection.type === 'activeSelection') {
        // multiple items selected
    } else {
        var panelNumber = panelGroups[selection.name].panelNumber;

        channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].xOffset = Math.round(selection.left / uiScale);
        channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].yOffset = Math.round(selection.top / uiScale);

        if (channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation == 'N')
            channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation = 'R';
        else if (channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation == 'R')
            channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation = 'U';
        else if (channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation == 'U')
            channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation = 'L';
        else if (channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation == 'L')
            channelOutputsLookup["LEDPanelMatrix"].panels[panelNumber].orientation = 'N';

        SetupCanvasPanel(panelNumber);
    }

    UpdateMatrixSize();
}

function GetOutputNumberColor(output) {
    switch (output) {
        case  0: return 'rgb(231, 76, 60)';
        case  1: return 'rgb( 88,214,141)';
        case  2: return 'rgb( 93,173,226)';
        case  3: return 'rgb(244,208, 63)';
        case  4: return 'rgb(229,152,102)';
        case  5: return 'rgb(195,155,211)';
        case  6: return 'rgb(162,217,206)';
        case  7: return 'rgb(174,182,191)';
        case  8: return 'rgb(212,239,223)';
        case  9: return 'rgb(246,221,204)';
        case 10: return 'rgb(245,183,177)';
        case 11: return 'rgb(183,149, 11)';
    }

    return 'rgb(255,  0,  0)';
}

function SetupCanvasPanel(panelNumber) {
    var co = channelOutputsLookup["LEDPanelMatrix"];
    var p = co.panels[panelNumber];
    var name = 'panel-' + panelNumber;

    if (name in panelGroups)
        ledPanelCanvas.remove(panelGroups[name].group);

    var w = co.panelWidth * uiScale;
    var h = co.panelHeight * uiScale;
    var a = 0;
    var tol = 0;
    var tot = 0;
    var aw = 9;
    var ah = 14;

    if ((p.orientation == 'R') ||
        (p.orientation == 'L')) {
        w = co.panelHeight * uiScale;
        h = co.panelWidth * uiScale;

        if (p.orientation == 'R') {
            a = 90;
            tot = 0;
            tol = ah;
        } else {
            a = 270;
            tot = aw;
            tol = 0;
        }
    } else if (p.orientation == 'U') {
        a = 180;
        tot = ah;
        tol = aw;
    }

    var panel = new fabric.Rect({
        fill: GetOutputNumberColor(p.outputNumber),
        originX: 'center',
        originY: 'center',
        width: w - 1,
        height: h - 1,
        stroke: 'black',
        strokeWidth: 1
    });

    var desc = 'O-' + (p.outputNumber+1) + ' P-' + (p.panelNumber+1) + '\n@ ' + p.xOffset + ',' + p.yOffset;
    var text = new fabric.Text(desc, {
        fontSize: 12,
        originX: 'center',
        originY: 'center'
    });

    var arrow = new fabric.Triangle({
        width: aw,
        height: ah,
        fill: 'black',
        left: tol - parseInt(w / 2 - 3),
        top: tot - parseInt(h / 2 - 3),
        angle: a
    });

    var group = new fabric.Group([ panel, text, arrow ], {
        left: p.xOffset * uiScale,
        top: p.yOffset * uiScale,
        borderColor: 'black',
        angle: 0,
        hasControls: false
    });

    group.toObject = (function(toObject) {
        return function() {
            return fabric.util.object.extend(toObject.call(this), {
                name: this.name,
                outputNumber: this.outputNumber,
                panelNumber: this.panelNumber,
                colorOrder: this.colorOrder,
                orientation: this.orientation
            });
        };
    })(group.toObject);

    group.name = name;
    group.outputNumber = p.outputNumber;
    group.panelNumber = p.panelNumber;
    group.colorOrder = p.colorOrder;
    group.orientation = p.orientation;

    var pg = {};
    pg.panelNumber = panelNumber;
    pg.group = group;
    pg.panel = panel;
    pg.text = text;
    pg.arrow = arrow;
    panelGroups[name] = pg;

    ledPanelCanvas.add(group);
    ledPanelCanvas.setActiveObject(group);
    panelSelectedHandler();
}

function UpdateMatrixSize() {
    var matrixWidth = 0;
    var matrixHeight = 0;

    for (var key in panelGroups) {
        var panel = new Object();
        var pg = panelGroups[key];

        if ((channelOutputsLookup["LEDPanelMatrix"].panels[pg.panelNumber].orientation == 'N') ||
            (channelOutputsLookup["LEDPanelMatrix"].panels[pg.panelNumber].orientation == 'U')) {
            if ((Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelWidth) > matrixWidth)
                matrixWidth = Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelWidth;
            if ((Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelHeight) > matrixHeight)
                matrixHeight = Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelHeight;
        } else {
            if ((Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelHeight) > matrixWidth)
                matrixWidth = Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelHeight;
            if ((Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelWidth) > matrixHeight)
                matrixHeight = Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrix"].panelWidth;
        }
    }

    var matrixPixels = matrixWidth * matrixHeight;
    var matrixChannels = matrixPixels * 3;

	$('#matrixSize').html('' + matrixWidth + 'x' + matrixHeight);
	$('#LEDPanelsChannelCount').html(matrixChannels);
	$('#LEDPanelsPixelCount').html(matrixPixels);

    var resized = 0;
    if (matrixWidth > parseInt($('#LEDPanelUIPixelsWide').val())) {
        resized = 1;
        $('#LEDPanelUIPixelsWide').val(matrixWidth);
    }
    if (matrixHeight > parseInt($('#LEDPanelUIPixelsHigh').val())) {
        resized = 1;
        $('#LEDPanelUIPixelsHigh').val(matrixHeight);
    }
    if (resized)
        SetCanvasSize();
}

function SetCanvasSize() {
    canvasWidth = parseInt($('#LEDPanelUIPixelsWide').val()) * uiScale;
    canvasHeight = parseInt($('#LEDPanelUIPixelsHigh').val()) * uiScale;

    ledPanelCanvas.setWidth(canvasWidth);
    ledPanelCanvas.setHeight(canvasHeight);
    ledPanelCanvas.calcOffset();
    ledPanelCanvas.renderAll();
}

function InitializeCanvas(reinit) {
    canvasInitialized = 1;

    if (reinit) {
        selectedPanel = -1;
        for (var key in panelGroups) {
            ledPanelCanvas.remove(panelGroups[key].group);
        }
        panelGroups = [];
    } else {
        ledPanelCanvas = new fabric.Canvas('ledPanelCanvas',
            {
                backgroundColor: '#a0a0a0',
                selection: false
            });
    }

    SetCanvasSize();

    if (channelOutputsLookup.hasOwnProperty("LEDPanelMatrix")) {
        for (var i = 0; i < channelOutputsLookup["LEDPanelMatrix"].panels.length; i++) {
            SetupCanvasPanel(i);
        }
    }

    UpdateMatrixSize();

    ledPanelCanvas.on('object:moving', panelMovingHandler);
    ledPanelCanvas.on('selection:created', panelSelectedHandler);
    ledPanelCanvas.on('selection:updated', panelSelectedHandler);
    ledPanelCanvas.on('selection:cleared', panelSelectedHandler);
}

function ToggleAdvancedLayout() {
    if ($('#LEDPanelUIAdvancedLayout').is(":checked")) {
        if (typeof channelOutputsLookup["LEDPanelMatrix"] === 'undefined') {
            SaveChannelOutputsJSON();
        }

        $('#LEDPanelUIPixelsWide').val(LEDPanelWidth * (LEDPanelCols + 1));
        $('#LEDPanelUIPixelsHigh').val(LEDPanelHeight * (LEDPanelRows + 1));

        if (canvasInitialized)
            InitializeCanvas(1);
        else
            InitializeCanvas(0);

        UpdateMatrixSize();
        $('.ledPanelSimpleUI').hide();
        $('.ledPanelCanvasUI').show();
    } else {
        UpdateLegacyLEDPanelLayout();
        $('.ledPanelCanvasUI').hide();
        $('.ledPanelSimpleUI').show();
    }
}

function cpOutputNumberChanged() {
    if (selectedPanel < 0)
        return;

    var co = channelOutputsLookup["LEDPanelMatrix"].panels[selectedPanel].outputNumber = parseInt($('#cpOutputNumber').val());
    SetupCanvasPanel(selectedPanel);
}

function cpPanelNumberChanged() {
    if (selectedPanel < 0)
        return;

    var co = channelOutputsLookup["LEDPanelMatrix"].panels[selectedPanel].panelNumber = parseInt($('#cpPanelNumber').val());
    SetupCanvasPanel(selectedPanel);
}

function cpColorOrderChanged() {
    if (selectedPanel < 0)
        return;

    var co = channelOutputsLookup["LEDPanelMatrix"].panels[selectedPanel].colorOrder = $('#cpColorOrder').val();
    SetupCanvasPanel(selectedPanel);
}

function SetupAdvancedUISelects() {
    for (var i = 0; i < LEDPanelOutputs; i++) {
        $('#cpOutputNumber').append("<option value='" + i + "'>" + (i+1) + "</option>");
    }

    for (var i = 0; i < LEDPanelPanelsPerOutput; i++) {
        $('#cpPanelNumber').append("<option value='" + i + "'>" + (i+1) + "</option>");
    }
}

function SetSinglePanelSize() {
    if (!("LEDPanelMatrix" in channelOutputsLookup))
        return;

    var w = channelOutputsLookup["LEDPanelMatrix"].panelWidth;
    var h = channelOutputsLookup["LEDPanelMatrix"].panelHeight;
    var s = channelOutputsLookup["LEDPanelMatrix"].panelScan;
    var singlePanelSize = [w, h, s].join('x');
    if ("panelAddressing" in channelOutputsLookup) {
        singlePanelSize = [singlePanelSize, channelOutputsLookup["LEDPanelMatrix"].panelAddressing].join('x');
    }
    $('#LEDPanelsSize').val(singlePanelSize.toString());
}

function PanelSubtypeChanged() {
    var select = document.getElementsByClassName("printSettingFieldCola col-md-3 col-lg-3")
    var html = "";

    html += "<select id='LEDPanelsSize' onchange='LEDPanelsSizeChanged();'>"

    <?if ($settings['Platform'] == "BeagleBone Black") {?>
    html += "<option value='32x16x8'>32x16 1/8 Scan</option>"
    html += "<option value='32x16x4'>32x16 1/4 Scan</option>"
    html += "<option value='32x16x4x1'>32x16 1/4 Scan ABCD</option>"
    html += "<option value='32x16x2'>32x16 1/2 Scan A</option>"
    html += "<option value='32x16x2x1'>32x16 1/2 Scan AB</option>"
    html += "<option value='32x32x16'>32x32 1/16 Scan</option>"
    html += "<option value='64x32x16'>64x32 1/16 Scan</option>"
    <?if ($panelCapesHaveSel4) {?>
    html += "<option value='64x64x32'>64x64 1/32 Scan</option>"
    <?}?>
    html += "<option value='64x32x8'>64x32 1/8 Scan</option>"
    html += "<option value='32x32x8'>32x32 1/8 Scan</option>"
    html += "<option value='40x20x5'>40x20 1/5 Scan</option>"
    html += "<option value='80x40x10'>80x40 1/10 Scan</option>"
    <?if ($panelCapesHaveSel4) {?>
    html += "<option value='80x40x20'>80x40 1/20 Scan</option>"
    <?}?>
    <?} else {?>
    html +="<option value='32x16x8'>32x16 1/8 Scan</option>"
    html +="<option value='32x16x4'>32x16 1/4 Scan</option>"
    html +="<option value='32x16x2'>32x16 1/2 Scan</option>"
    html +="<option value='32x32x16'>32x32 1/16 Scan</option>"
    html +="<option value='32x32x8'>32x32 1/8 Scan</option>"
    html +="<option value='64x32x16'>64x32 1/16 Scan</option>"
    html +="<option value='64x32x8'>64x32 1/8 Scan</option>"
    html +="<option value='64x64x8'>64x64 1/8 Scan</option>"
    if ($('#LEDPanelsConnection').val() === 'ColorLight5a75') {
        html += "<option value='48x48x6'>48x48 1/6 Scan</option>"
        html += "<option value='80x40x10'>80x40 1/10 Scan</option>"
        html += "<option value='80x40x20'>80x40 1/20 Scan</option>"
    }
    html += "<option value='128x64x32'>128x64 1/32 Scan</option>"
    <?}?>

    html += "</select>"

    select.item(0).innerHTML = html

    SetSinglePanelSize();
}

function AutoLayoutPanels() {
    var sure = prompt('WARNING: This will re-configure all output and panel numbers with one output per row and all panels facing in an upwards direction.  Are you sure? [Y/N]', 'N');

    if (sure.toUpperCase() != 'Y')
        return;

    var panelsWide = parseInt($('#LEDPanelsLayoutCols').val());
    var panelsHigh = parseInt($('#LEDPanelsLayoutRows').val());

    for (var y = 0; y < panelsHigh; y++) {
        for (var x = 0; x < panelsWide; x++) {
            var panel = panelsWide - x - 1;
            $('#LEDPanelPanelNumber_' + y + '_' + panel).val(x);
            $('#LEDPanelOutputNumber_' + y + '_' + panel).val(y);
            $('#LEDPanelOrientation_' + y + '_' + panel).attr('src', 'images/arrow_N.png');
            $('#LEDPanelColorOrder_' + y + '_' + panel).val('');
        }
    }
    SaveChannelOutputsJSON();
}
function TogglePanelTestPattern() {
    var val = $("#PanelTestPatternButton").val();
    if (val == "Test Pattern") {
        var outputType = $('#LEDPanelsConnection').val();
        if (outputType == "ColorLight5a75") {
            outputType = "ColorLight Panels";
        } else if (outputType == "RGBMatrix") {
            outputType = "Pi Panels";
        } else if (outputType == "BBBMatrix") {
            outputType = "BBB Panels";
        }
        $("#PanelTestPatternButton").val("Stop Pattern");
        var data = '{"command":"Test Start","multisyncCommand":false,"multisyncHosts":"","args":["1000","Output Specific","' + outputType + '","1"]}';
        $.post("api/command", data
	    ).done(function(data) {
	    }).fail(function() {
	    });
    } else {
        $("#PanelTestPatternButton").val("Test Pattern");
        var data = '{"command":"Test Stop","multisyncCommand":false,"multisyncHosts":"","args":[]}';
        $.post("api/command", data
	    ).done(function(data) {
	    }).fail(function() {
	    });
    }
}

function WarnIfSlowNIC() {
    var NicSpeed = parseInt($('#LEDPanelsInterface').find(":selected").text().split('(')[1].split('M')[0]);
    if (NicSpeed < 1000 && $('#LEDPanelsConnection').find(":selected").text()=="ColorLight" && $('#LEDPanelsEnabled').is(":checked")==true) {
        $('#divLEDPanelWarnings').html('<div class="alert alert-danger">Selected interface does not support 1000+ Mbps, which is the Colorlight minimum</div>');
    } else
    {
        $('#divLEDPanelWarnings').html("");
    }
}

$(document).ready(function(){
	InitializeLEDPanels();
	LEDPanelsConnectionChanged();

    SetupAdvancedUISelects();

    if ($('#LEDPanelUIAdvancedLayout').is(":checked")) {
        InitializeCanvas(0);
        $('.ledPanelSimpleUI').hide();
        $('.ledPanelCanvasUI').show();
    } else {
        $('.ledPanelCanvasUI').hide();
    }

<?
if ((isset($settings['cape-info'])) &&
    ((in_array('all', $settings['cape-info']["provides"])) ||
        (in_array('panels', $settings['cape-info']["provides"])))) {
    ?>
    if (currentCapeName != "" && currentCapeName != "Unknown") {
        $('.capeNamePanels').html(currentCapeName);
        $('.capeTypeLabel').html("Cape Config");
    }

<?
}
?>
WarnIfSlowNIC();
});

</script>

	<div id='divLEDPanels'>
        <div class="row tableTabPageHeader">
            <div class="col-md"><h2><span class='capeNamePanels'>LED Panels</span> </h2></div>
            <div class="col-md-auto ms-lg-auto">
                <div class="form-actions">
                    <input id="PanelTestPatternButton" type='button' class="buttons ms-1" onClick='TogglePanelTestPattern();' value='Test Pattern'>
                    <input type='button' class="buttons btn-success ms-1" onClick='SaveChannelOutputsJSON();' value='Save'>
                </div>
            </div>
        </div>
        <div id="divLEDPanelWarnings">

        </div>
        <div class="backdrop tableOptionsForm">
            <div class="row">
                <div class="col-md-auto">
                    <div class="backdrop-dark form-inline enableCheckboxWrapper">
                        <div><b>Enable <span class='capeNamePanels'>Led Panels</span>:&nbsp;</b></div>
                        <div><input id='LEDPanelsEnabled' type='checkbox' onChange="WarnIfSlowNIC();"></div>
                    </div>
                </div>
                <div class="col-md-auto form-inline">
                    <div id='LEDPanelsConnectionLabel'><b>Connection:&nbsp;</b></div>
                    <div>
                    <select id='LEDPanelsConnection' onChange='LEDPanelsConnectionChanged();'>
<?
if (in_array('all', $currentCapeInfo["provides"])
    || in_array('panels', $currentCapeInfo["provides"])) {
    if ($settings['Platform'] == "Raspberry Pi") {
        ?>
                            <option value='RGBMatrix'>Hat/Cap/Cape</option>
<?
    } else if ($settings['Platform'] == "BeagleBone Black") {?>
                            <option value='LEDscapeMatrix'>Hat/Cap/Cape</option>
<?}?>
<?
}?>
                            <option value='ColorLight5a75'>ColorLight</option>
<?
if ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux")) {
    echo "<option value='X11PanelMatrix'>X11 Panel Matrix</option>\n";
}
?>
                       </select>
                    </div>
                </div>
                <div class="col-md-auto form-inline" id="LEDPanelsConnectionInterface">
                    <b>Interface:</b>
                    <select id='LEDPanelsInterface' type='hidden' onChange='WarnIfSlowNIC();'>
                        <?PopulateEthernetInterfaces();?>
                    </select>
                </div>
                <div class="col-md-auto form-inline" id="LEDPanelsWiringPinoutLabel">
                    <b>Wiring Pinout:</b>
                    <select id='LEDPanelsWiringPinout'>
                                <?if ($settings['Platform'] == "Raspberry Pi") {?>
                                    <option value='regular'>Standard</option>
                                    <option value='adafruit-hat'>Adafruit</option>
                                    <option value='adafruit-hat-pwm'>Adafruit PWM</option>
                                    <option value='regular-pi1'>Standard Pi1</option>
                                    <option value='classic'>Classic</option>
                                    <option value='classic-pi1'>Classic Pi1</option>
                                <?} else if ($settings['Platform'] == "BeagleBone Black") {
    if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) {?>
                                          <option value='v2'>v2.x</option>
                                <?} else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {?>
                                          <option value='PocketScroller1x'>PocketScroller</option>
                                <?} else {?>
                                          <option value='v1'>Standard v1.x</option>
                                          <option value='v2'>v2.x</option>
                                <?}?>
                                <?
}?>
					</select>
                </div>
            </div>
        </div>
        <div id='divLEDPanelsData'>
            <div class="container-fluid settingsTable">
                <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span class='ledPanelSimpleUI'><b>Panel Layout:</b></span><span class='ledPanelCanvasUI'><b>Matrix Size (WxH):</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><span class='ledPanelSimpleUI'><?printLEDPanelLayoutSelect();?> <input type='button' class='buttons' onClick='AutoLayoutPanels();' value='Auto Layout'></span>
                                    <span class='ledPanelCanvasUI'><span id='matrixSize'></span></span></div>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Start Channel:</b></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><input id='LEDPanelsStartChannel' type=number min=1 max=<?=FPPD_MAX_CHANNELS?> value='1'></div>
                </div>
                <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Single Panel Size (WxH):</b></div>
                    <div class="printSettingFieldCola col-md-3 col-lg-3">
                        <script>
                            function LEDPanelsSizeChanged() {
                                var value = $('#LEDPanelsSize').val();
                                SetSetting("LEDPanelsSize", value, 1, 0, false, null, function() {
                                        settings['LEDPanelsSize'] = value;
                                        LEDPanelLayoutChanged();
                                });
                            }
                        </script>
                    </div>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Channel Count:</b></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><span id='LEDPanelsChannelCount'>1536</span>(<span id='LEDPanelsPixelCount'>512</span> Pixels)</div>
                </div>
                <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Model Start Corner:</b></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select id='LEDPanelsStartCorner'>
							<option value='0'>Top Left</option>
							<option value='1'>Bottom Left</option>
						</select></div>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Default Panel Color Order:</b></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select id='LEDPanelsColorOrder'>
                            <option value='RGB'>RGB</option>
                            <option value='RBG'>RBG</option>
                            <option value='GRB'>GRB</option>
                            <option value='GBR'>GBR</option>
                            <option value='BRG'>BRG</option>
                            <option value='BGR'>BGR</option>
						</select>
                    </div>
                </div>
                <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Panel Gamma:</b></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><?printLEDPanelGammaSelect($settings['Platform'], $LEDPanelGamma);?></div>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                </div>
                <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsBrightnessLabel'><b>Brightness:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select id='LEDPanelsBrightness'>
                                <?
if ($settings['Platform'] == "Raspberry Pi") {
    for ($x = 100; $x >= 10; $x -= 5) {
        echo "<option value='$x'>$x%</option>\n";
    }

} else {
    for ($x = 10; $x >= 1; $x -= 1) {
        echo "<option value='$x'>$x</option>\n";
    }

}
?>
						</select>
                    </div>
    <?if ($settings['Platform'] == "Raspberry Pi") {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsGPIOSlowdownLabel'><b>GPIO Slowdown:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select id='LEDPanelsGPIOSlowdown'>
                            <option value='0'>0 (Pi Zero and other single-core)</option>
                            <option value='1' selected>1 (multi-core Pi)</option>
                            <option value='2'>2 (slow panels)</option>
                            <option value='3'>3 (slower panels)</option>
                            <option value='4'>4 (slower panels or faster Pi)</option>
                            <option value='5'>5 (slower panels or faster Pi)</option>
                        </select>
                    </div>

    <?} else if ($settings['Platform'] == "BeagleBone Black") {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsOutputByRowLabel'><b>Output By Row:</b></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><input id='LEDPanelsOutputByRow' type='checkbox' onclick='outputByRowClicked()'></div>
    <?} else {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
    <?}?>

                </div>
        <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsInterleaveLabel'><b>Panel Interleave:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <?printLEDPanelInterleaveSelect($settings['Platform']);?>
                    </div>
            <?if ($settings['Platform'] == "Raspberry Pi") {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsOutputCPUPWMLabel'><b>Use CPU PWM:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><input id='LEDPanelsOutputCPUPWM' type='checkbox'></div>
            <?} else if ($settings['Platform'] == "BeagleBone Black") {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsOutputBlankRowLabel'><b>Blank between rows:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"><input id='LEDPanelsOutputBlankRow' type='checkbox'></div>
            <?} else {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
            <?}?>
                </div>
                <div class="row">
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsColorDepthLabel'><b>Color Depth:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select id='LEDPanelsColorDepth'>
                            <?if ($settings['Platform'] == "BeagleBone Black") {?>
                                    <option value='12'>12 Bit</option>
                            <?}?>
                                    <option value='11'>11 Bit</option>
                                    <option value='10'>10 Bit</option>
                                    <option value='9'>9 Bit</option>
									<option value='8' selected>8 Bit</option>
									<option value='7'>7 Bit</option>
									<option value='6'>6 Bit</option>
								</select>
                    </div>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                </div>
            </div>
        </div>
        <div class="row">
            <?if ($settings['Platform'] == "Raspberry Pi") {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span id='LEDPanelsRowAddressTypeLabel'><b>Panel Row Address Type:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select id='LEDPanelRowAddressType'>
                            <option value='0' selected>Standard</option>
                            <option value='1'>AB-Addressed Panels</option>
                            <option value='2'>Direct Row Select</option>
                            <option value='3'>ABC-Addressed Panels</option>
                            <option value='4'>ABC Shift + DE Direct</option>
                        </select>
                    </div>
            <?} else {?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
            <?}?>
        </div>
  </div>

		<div id='divLEDPanelsLayoutData'>
			<div style="padding: 10px;">
                <br>
                <b>LED Panel Layout:</b><br>
                Advanced Layout?
                <?PrintSettingCheckbox("Advanced Layout", "LEDPanelUIAdvancedLayout", 0, 0, "1", "0", "", "ToggleAdvancedLayout", 0);?>
                <br>
                <span class='ledPanelSimpleUI'>
                View Config from front?
                <?PrintSettingCheckbox("Front View", "LEDPanelUIFrontView", 0, 0, "1", "0", "", "FrontBackViewToggled", 1);?>
                </span>
                <span class='ledPanelCanvasUI'>Front View</span>
                <br>
                <div style='overflow: auto;'>
                    <table class='ledPanelCanvasUI' style='display:none;'>
                        <tr>
                            <td><canvas id='ledPanelCanvas' width='900' height='400' style='border: 2px solid rgb(0,0,0);'></canvas></td>
                            <td></td>
                            <td>
                                <b>Selected Panel:</b><br>
                                <table>
                                    <tr><td>Output:</td><td>
                                        <select id='cpOutputNumber' onChange='cpOutputNumberChanged();'>
                                        </select></td></tr>
                                    <tr><td>Panel:</td><td>
                                        <select id='cpPanelNumber' onChange='cpPanelNumberChanged();'>
                                        </select></td></tr>
                                    <tr><td>Color:</td><td>
                                        <select id='cpColorOrder' onChange='cpColorOrderChanged();'>
                                            <option value=''>Def</option>
                                            <option value='RGB'>RGB</option>
                                            <option value='RBG'>RBG</option>
                                            <option value='GBR'>GBR</option>
                                            <option value='GRB'>GRB</option>
                                            <option value='BGR'>BGR</option>
                                            <option value='BRG'>BRG</option>
                                        </select></td></tr>
                                    <tr><td>X Pos:</td><td><span id='cpXOffset'></span></td></tr>
                                    <tr><td>Y Pos:</td><td><span id='cpYOffset'></span></td></tr>
                                    <tr><td colspan=2><input type='button' onclick='RotateCanvasPanel();' value='Rotate'></td></tr>
                                    <!--
                                    <tr><td>Snap:</td><td><input type='checkbox' id='cpSnap'></td></tr>
                                    -->
                                </table>
                            </td>
                        </tr>
                        <tr>
                            <td colspan=3>
                                <table>
                                    <tr>
                                        <td><b>UI Layout Size:</b></td>
                                        <td>Pixels Wide:</td><td><?PrintSettingTextSaved("LEDPanelUIPixelsWide", 2, 0, 4, 4, "", "256", "SetCanvasSize");?></td>
                                        <td></td>
                                        <td>Pixels High:</td><td><?PrintSettingTextSaved("LEDPanelUIPixelsHigh", 2, 0, 4, 4, "", "128", "SetCanvasSize");?></td>
                                    </tr>
                                </table>
                            </td>
                        </tr>
                    </table>
                    <div class='ledPanelSimpleUI'>
                        <table id='LEDPanelTable'>
                            <tbody>
                            </tbody>
                        </table>
                    </div>
				    - O-# is physical output number.<br>
			    	- P-# is panel number on physical output.<br>
		    		- C-(color) is color order if panel has different color order than default (C-Def).<br>
	    			- Arrow <img src='images/arrow_N.png' height=17 width=17 alt="panel orientation"> indicates panel orientation, click arrow to rotate.<br>
                </div>
                <br>
                <b>Notes and hints:</b>
                <ul>
<?if (count($panelCapes) == 1) {
    if (isset($panelCapes[0]["warnings"][$settings["SubPlatform"]])) {
        echo "<li><font color='red'>" . $panelCapes[0]["warnings"][$settings["SubPlatform"]] . "</font></li>\n";
    }
    if (isset($panelCapes[0]["warnings"]["all"])) {
        echo "<li><font color='red'>" . $panelCapes[0]["warnings"]["all"] . "</font></li>\n";
    }
    if (isset($panelCapes[0]["warnings"]["*"])) {
        echo "<li><font color='red'>" . $panelCapes[0]["warnings"]["*"] . "</font></li>\n";
    }
}?>
                    <li><font color='red'>New Colorlight receiver firmware eg 13.x is currently incompatible with FPP, please see <a href="https://github.com/FalconChristmas/fpp/issues/1849">Issue 1849</a></font></li>
                    <li>When wiring panels, divide the panels across as many outputs as possible.  Shorter chains on more outputs will have higher refresh than longer chains on fewer outputs.</li>
                    <li>If not using all outputs, use all the outputs from 1 up to what is needed.   Data is always sent on outputs up to the highest configured, even if no panels are attached.</li>
                    <?if ($settings['Platform'] == "Raspberry Pi") {?>
                    <li>The FPP developers strongly encourage using either a BeagleBone based panel driver (Octoscroller, PocketScroller) or using a ColorLight controller.  The Raspberry Pi panel code performs poorly compared to the other options and supports a much more limited set of options.</li>
                    <?}?>
                </ul>
		    </div>
	    </div>




