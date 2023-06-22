<!DOCTYPE html>
<html>
<head>
<?php
require_once "common.php";
require_once 'config.php';
require_once 'universeentry.php';
if (file_exists(__DIR__ . "/fppdefines.php")) {
    include_once __DIR__ . '/fppdefines.php';
} else {
    include_once __DIR__ . '/fppdefines_unknown.php';
}
include 'common/menuHead.inc';

//$settings['Platform'] = "Raspberry Pi"; // Uncomment for testing
//$settings['Platform'] = "BeagleBone Black"; // Uncomment for testing
//$settings['SubPlatform'] = "BeagleBone Black"; // Uncomment for testing
?>
<script>
var currentCapeName = '';
</script>

<?
$currentCape = "";
$currentCapeInfo = json_decode("{ \"provides\": [\"all\"]}", true);
if (isset($settings['cape-info'])) {
    $currentCapeInfo = $settings['cape-info'];
    $currentCape = $currentCapeInfo["id"];
    echo "<!-- current cape is " . $currentCape . "-->\n";
    printf("<script>\ncurrentCapeName = '%s';\n</script>\n",
        $currentCapeInfo["name"]);
}
if (!isset($currentCapeInfo['provides'])) {
    $currentCapeInfo['provides'][] = "all";
    if (isset($settings['FalconHardwareDetected']) && ($settings['FalconHardwareDetected'] == 1)) {
        $currentCapeInfo['provides'][] = "fpd";
    }
} else if (isset($settings["showAllOptions"]) && $settings["showAllOptions"] == 1) {
    $currentCapeInfo['provides'][] = "all";
    $currentCapeInfo['provides'][] = "fpd";
} else if ($uiLevel >= 2) {
    $currentCapeInfo['provides'][] = "all";
} else if ($uiLevel == 1) {
    if (!isset($currentCapeInfo['provides']) || count($currentCapeInfo['provides']) == 0) {
        $currentCapeInfo['provides'][] = "all";
    } else {
        $currentCapeInfo['provides'][] = "udp";
    }
}

?>

<script language="Javascript">

var channelOutputs = [];
var channelOutputsLookup = [];
var currentTabType = 'UDP';

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
	}
}

function GetChannelOutputConfig()
{
	var config = new Object();

	config.channelOutputs = [];

    var lpc = GetLEDPanelConfig();
	config.channelOutputs.push(lpc);

    channelOutputs = config;
    UpdateChannelOutputLookup();
	var result = JSON.stringify(config);
	return result;
}

function SaveChannelOutputsJSON()
{
	var configStr = GetChannelOutputConfig();
	$.post("api/configfile/channeloutputs.json",
        configStr
	).done(function(data) {
		$.jGrowl(" Channel Output configuration saved",{themeState:'success'});
		SetRestartFlag(1);
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
if (file_exists($settings['channelOutputsJSON'])) {
    $channelOutputsJSON = file_get_contents($settings['channelOutputsJSON']);
    $channelOutputs = json_decode($channelOutputsJSON, true);
    $channelOutputsJSON = preg_replace("/\n|\r/", "", $channelOutputsJSON);
    $channelOutputsJSON = preg_replace("/\"/", "\\\"", $channelOutputsJSON);
}

// If led panels are enabled, make sure the page is displayed even if the cape is a string cape (could be a colorlight output)
if ($channelOutputs != null && $channelOutputs['channelOutputs'] != null && $channelOutputs['channelOutputs'][0] != null) {
    if ($channelOutputs['channelOutputs'][0]['type'] == "LEDPanelMatrix" && $channelOutputs['channelOutputs'][0]['enabled'] == 1) {
        $currentCapeInfo["provides"][] = 'panels';
    }
}

?>

function handleCOKeypress(e)
{
	if (e.keyCode == 113) {
		if ( $('.nav-link.active').attr('tabtype')=='strings' ){
			setPixelStringsStartChannelOnNextRow();
		}
	}
}


$(document).ready(function(){
	$(document).on('keydown', handleCOKeypress);

	var channelOutputsJSON = "<?echo $channelOutputsJSON; ?>";

	if (channelOutputsJSON != "")
		channelOutputs = jQuery.parseJSON(channelOutputsJSON);
	else
		channelOutputs.channelOutputs = [];

	UpdateChannelOutputLookup();

});

</script>
<title><?echo $pageTitle; ?></title>
</head>
<body>
	<div id="bodyWrapper">
		<?php
$activeParentMenuItem = 'input-output';
include 'menu.inc';?>
  <div class="mainContainer">

<div id='channelOutputManager'>
		<h1 class='title'>Channel Outputs</h1>
		<div class="pageContent">
<?
$lpTabStyle = " hidden";
$stringTabStyle = " hidden";
$e131TabStyle = " hidden";
$e131TabStyleActive = "";
$lpTabStyleActive = "";
$stringTabStyleActive = "";

if (in_array('all', $currentCapeInfo["provides"])) {
    $lpTabStyle = "";
    $e131TabStyle = "";
    $stringTabStyle = "";
} else {
    if (in_array('udp', $currentCapeInfo["provides"])) {
        $e131TabStyle = "";
    }
    if (in_array('strings', $currentCapeInfo["provides"])) {
        $stringTabStyle = "";
    }
    if (in_array('panels', $currentCapeInfo["provides"])
        || !in_array('strings', $currentCapeInfo["provides"])) {
        $lpTabStyle = "";
    }
    if (!in_array('panels', $currentCapeInfo["provides"])
        && !in_array('strings', $currentCapeInfo["provides"])) {
        $e131TabStyle = "";
    }
}
if ($e131TabStyle == "") {
    $e131TabStyleActive = "active";
} else if ($stringTabStyle == "") {
    $stringTabStyleActive = "active";
} else if ($lpTabStyle == "") {
    $lpTabStyleActive = "active";
}
?>

			<ul class="nav nav-pills pageContent-tabs" id="channelOutputTabs" role="tablist">
              <li class="nav-item <?=$e131TabStyle?>" id="tab-e131-LI">
                <a class="nav-link <?=$e131TabStyleActive?>" id="tab-e131-tab" tabType='UDP' data-bs-toggle="pill" href="#tab-e131" role="tab" aria-controls="tab-e131" aria-selected="true">
					E1.31 / ArtNet / DDP / KiNet
				</a>
              </li>

		<?

if ($settings['Platform'] == "BeagleBone Black" || $settings['Platform'] == "Raspberry Pi" ||
    ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux"))) {
    $stringTabText = "Pixel Strings";
    if (in_array('all', $currentCapeInfo["provides"]) || in_array('strings', $currentCapeInfo["provides"])) {
        if ((isset($settings['cape-info'])) &&
            ((in_array('all', $settings['cape-info']["provides"])) || (in_array('strings', $settings['cape-info']["provides"]))) &&
            (isset($currentCapeInfo["name"]) && $currentCapeInfo["name"] != "Unknown")) {
            $stringTabText = $currentCapeInfo["name"];
            if (in_array('all', $settings['cape-info']["provides"]) || in_array('panels', $settings['cape-info']["provides"])) {
                $stringTabText .= " Pixel Strings";
            }

        }
    }
    ?>
                <li class="nav-item <?=$stringTabStyle?>" id="tab-strings-LI" >
                    <a class="nav-link <?=$stringTabStyleActive?>" id="stringTab-tab" tabType='strings' data-bs-toggle="pill" href='#stringTab' role="tab" aria-controls="stringTab">
                        <?echo $stringTabText; ?>
                    </a>
                </li>
                <?
}
if ($settings['Platform'] == "Raspberry Pi") {
    if (in_array('fpd', $currentCapeInfo["provides"])) {
        ?>
						<li class="nav-item">
							<a class="nav-link" id="tab-fpd-tab" tabType='FPD' data-bs-toggle="pill" href='#tab-fpd' role="tab" aria-controls="tab-fpd">
								Falcon Pixelnet/DMX
							</a>
						</li>
					<?
    }
}

$ledTabText = "LED Panels";
if ((isset($settings['cape-info'])) &&
    ((in_array('all', $settings['cape-info']["provides"])) || (in_array('panels', $settings['cape-info']["provides"]))) &&
    (isset($currentCapeInfo["name"]) && $currentCapeInfo["name"] != "Unknown")) {
    $ledTabText = $currentCapeInfo["name"];
    if (in_array('all', $settings['cape-info']["provides"]) || in_array('strings', $settings['cape-info']["provides"])) {
        $ledTabText .= " LED Panels";
    }

}
?>
						<li class="nav-item <?=$lpTabStyle?>" id="tab-LEDPanels-LI">
							<a class="nav-link <?=$lpTabStyleActive?>" id="tab-LEDPanels-tab" tabType='panels' data-bs-toggle="pill" href='#tab-LEDPanels' role="tab" aria-controls="tab-LEDPanels">
                                <?echo $ledTabText; ?>
							</a>
						</li>
						<li class="nav-item">
							<a class="nav-link" id="tab-other-tab" tabType='other' data-bs-toggle="pill" href='#tab-other' role="tab" aria-controls="tab-other">
								Other
							</a>
						</li>
					</ul>

		<!-- --------------------------------------------------------------------- -->
<?
if ($e131TabStyle == "") {
    $e131TabStyleActive = "show active";
} else if ($stringTabStyle == "") {
    $stringTabStyleActive = "show active";
} else if ($lpTabStyle == "") {
    $lpTabStyleActive = "show active";
}
?>

		<div class="tab-content" id="channelOutputTabsContent">
			<div class="tab-pane fade <?=$e131TabStyleActive?>" id="tab-e131" role="tabpanel" aria-labelledby="tab-e131-tab">
				<?include_once 'co-universes.php';?>
			</div>

			<div class="tab-pane fade" id="pills-contact" role="tabpanel" aria-labelledby="pills-contact-tab">Tab 3</div>


		<?

if ($settings['Platform'] == "BeagleBone Black" || $settings['Platform'] == "Raspberry Pi" ||
    ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux"))) {
    ?>
            <div class="tab-pane fade <?=$stringTabStyleActive?>" id="stringTab" role="tabpanel" aria-labelledby="stringTab-tab">
                <?include_once 'co-pixelStrings.php';?>
            </div>
            <?
}
if ($settings['Platform'] == "Raspberry Pi") {
    if (in_array('fpd', $currentCapeInfo["provides"])) {
        ?>
					<div class="tab-pane fade" id="tab-fpd" role="tabpanel" aria-labelledby="tab-fpd-tab">
						<?include_once 'co-fpd.php';?>
					</div>
				<?
    }
}
?>

            <div class="tab-pane fade <?=$lpTabStyleActive?>" id="tab-LEDPanels" role="tabpanel" aria-labelledby="tab-LEDPanels-tab">
                <?include_once 'co-ledPanels.php';?>
            </div>
			<div class="tab-pane fade" id="tab-other" role="tabpanel" aria-labelledby="tab-other-tab">
				<?include_once "co-other.php";?>
			</div>

				</div>
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
</div>

	<?php	include 'common/footer.inc';?>
</div>
</body>
</html>
