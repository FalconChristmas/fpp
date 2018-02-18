<html>
<head>
<?php
require_once("common.php");
require_once('universeentry.php');
include 'common/menuHead.inc';

//$settings['Platform'] = "Raspberry Pi"; // Uncomment for testing
//$settings['Platform'] = "BeagleBone Black"; // Uncomment for testing
//$settings['SubPlatform'] = "BeagleBone Black"; // Uncomment for testing
?>
<script language="Javascript">

var channelOutputs = [];
var channelOutputsLookup = [];
var currentTabTitle = "E1.31 / ArtNet";

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
				channelOutputsLookup["LEDPanelColorOrder_" + r + "_" + c]
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

	var postData = "command=setChannelOutputs&file=channelOutputsJSON&data=" + JSON.stringify(configStr);

	$.post("fppjson.php", postData
	).success(function(data) {
		$.jGrowl(" Channel Output configuration saved");
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Channel Output Config", "Save Failed");
	});
}

function inputsAreSane()
{
	var result = 1;

	result &= pixelStringInputsAreSane();

	return result;
}

function okToAddNewInput(type)
{
	var result = 1;

	result &= okToAddNewPixelStringInput(type);

	return result;
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

function handleCOKeypress(e)
{
	if (e.keyCode == 113) {
		if (currentTabTitle == "Pi Pixel Strings")
			setPixelStringsStartChannelOnNextRow();
        if (currentTabTitle == "BBB Strings")
            setPixelStringsStartChannelOnNextRow();
	}
}


$(document).ready(function(){
	$(document).on('keydown', handleCOKeypress);

	var channelOutputsJSON = "<? echo $channelOutputsJSON; ?>";

	if (channelOutputsJSON != "")
		channelOutputs = jQuery.parseJSON(channelOutputsJSON);
	else
		channelOutputs.channelOutputs = [];

	UpdateChannelOutputLookup();

	// Init tabs
  $tabs = $("#tabs").tabs({
  		activate: function(e, ui) {
			currentTabTitle = $(ui.newTab).text();
		},
  		cache: true,
		spinner: "",
		fx: {
			opacity: 'toggle',
			height: 'toggle'
		}
	});

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

#tblUniverses th {
	vertical-align: bottom;
	text-align: center;
	border: solid 2px #888888;
}

#tblUniverses td {
	text-align: center;
}

#tblUniverses input[type=text] {
	text-align: center;
	width: 100%;
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
		echo "<li><a href='#tab-BBB48String'>BBB Strings</a></li>\n";
	}

	if ($settings['Platform'] == "Raspberry Pi")
	{
		echo "<li><a href='#tab-PixelStrings'>Pi Pixel Strings</a></li>\n";
	}
?>
				<li><a href='#tab-LEDPanels'>LED Panels</a></li>
				<li><a href="#tab-other">Other</a></li>
			</ul>

<!-- --------------------------------------------------------------------- -->

<?

include_once('co-universes.php');

if ($settings['Platform'] == "Raspberry Pi")
{
	include_once('co-fpd.php');
	include_once('co-piPixelString.php');
}

include_once('co-ledPanels.php');

if ($settings['Platform'] == "BeagleBone Black")
{
	include_once('co-bbbStrings.php');
}

include_once("co-other.php");
?>

<!-- --------------------------------------------------------------------- -->

</div>
</div>

<div id='debugOutput'>
</div>

<!-- FIXME, can we put this in co-ledPanels.php? -->
<div id="dialog-panelLayout" title="panelLayout" style="display: none">
  <div id="layoutText">
  </div>
</div>

	<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
