<?php
require_once('config.php');
require_once('common.php');

function PrintSequenceOptions()
{
	global $sequenceDirectory;
	$first = 1;
	echo "<select id=\"selSequence\" size=\"1\">";
	foreach(scandir($sequenceDirectory) as $seqFile) 
	{
		if($seqFile != '.' && $seqFile != '..' && !preg_match('/.eseq$/', $seqFile))
		{
			echo "<option value=\"" . $seqFile . "\"";
			if ($first)
			{
				echo " selected";
				$first = 0;
			}
			echo ">" . $seqFile . "</option>";
		}
	}
	echo "</select>";
}			

$rgbLabels = array();
$rgbColors = array();
$rgbStr    = "RGB";
$rgbColorList = "R-G-B";

if (isset($settings['useRGBLabels']) && ($settings['useRGBLabels'] == '0'))
{
	$rgbLabels[0] = 'A';
	$rgbLabels[1] = 'B';
	$rgbLabels[2] = 'C';
	$rgbColors[0] = 'A';
	$rgbColors[1] = 'B';
	$rgbColors[2] = 'C';
	$rgbStr       = "ABC";
	$rgbColorList = "A-B-C";
}
else
{
	$rgbLabels[0] = 'R';
	$rgbLabels[1] = 'G';
	$rgbLabels[2] = 'B';
	$rgbColors[0] = 'Red';
	$rgbColors[1] = 'Green';
	$rgbColors[2] = 'Blue';
	$rgbStr       = "RGB";
	$rgbColorList = "R-G-B";
	$settings['useRGBLabels'] = 1;
}

?>

<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
<style>

#testModeCycleMS {
  border-width: 1px;
  border-style: solid;
  border-color: #333 #333 #777 #333;
  border-radius: 25px;
  margin-left: 12px;
  margin-right: 12px;
  width: 300px;
  position: relative;
  height: 13px;
  background-color: #8e8d8d;
  background: url('../images/bg-track.png') repeat top left;
  box-shadow: inset 0 1px 5px 0px rgba(0, 0, 0, .5),
              0 1px 0 0px rgba(250, 250, 250, .5);
  left: 0px;
  top: 6px;
  float: left;
}

#testModeColorS {
  border-width: 1px;
  border-style: solid;
  border-color: #333 #333 #777 #333;
  border-radius: 25px;
  margin-left: 12px;
  margin-right: 12px;
  width: 300px;
  position: relative;
  height: 13px;
  background-color: #8e8d8d;
  background: url('../images/bg-track.png') repeat top left;
  box-shadow: inset 0 1px 5px 0px rgba(0, 0, 0, .5),
              0 1px 0 0px rgba(250, 250, 250, .5);
  left: 0px;
  top: 6px;
  float: left;
}

#testModeColor1 {
  border-width: 1px;
  border-style: solid;
  border-color: #333 #333 #777 #333;
  border-radius: 25px;
  margin-left: 12px;
  margin-right: 12px;
  width: 300px;
  position: relative;
  height: 13px;
  background-color: #8e8d8d;
  background: url('../images/bg-track.png') repeat top left;
  box-shadow: inset 0 1px 5px 0px rgba(0, 0, 0, .5),
              0 1px 0 0px rgba(250, 250, 250, .5);
  left: 0px;
  top: 6px;
  float: left;
}

#testModeColor2 {
  border-width: 1px;
  border-style: solid;
  border-color: #333 #333 #777 #333;
  border-radius: 25px;
  margin-left: 12px;
  margin-right: 12px;
  width: 300px;
  position: relative;
  height: 13px;
  background-color: #8e8d8d;
  background: url('../images/bg-track.png') repeat top left;
  box-shadow: inset 0 1px 5px 0px rgba(0, 0, 0, .5),
              0 1px 0 0px rgba(250, 250, 250, .5);
  left: 0px;
  top: 6px;
  float: left;
}

#testModeColor3 {
  border-width: 1px;
  border-style: solid;
  border-color: #333 #333 #777 #333;
  border-radius: 25px;
  margin-left: 12px;
  margin-right: 12px;
  width: 300px;
  position: relative;
  height: 13px;
  background-color: #8e8d8d;
  background: url('../images/bg-track.png') repeat top left;
  box-shadow: inset 0 1px 5px 0px rgba(0, 0, 0, .5),
              0 1px 0 0px rgba(250, 250, 250, .5);
  left: 0px;
  top: 6px;
  float: left;
}

.rgbCustomColor {
  border-width: 1px;
  border-style: solid;
  border-color: #333 #333 #777 #333;
  border-radius: 25px;
  margin-left: 12px;
  margin-right: 12px;
  width: 300px;
  position: relative;
  height: 13px;
  background-color: #8e8d8d;
  background: url('../images/bg-track.png') repeat top left;
  box-shadow: inset 0 1px 5px 0px rgba(0, 0, 0, .5),
              0 1px 0 0px rgba(250, 250, 250, .5);
  left: 0px;
  top: 6px;
  float: left;
}

</style>
</head>
<body onunload='DisableTestMode();'>

<script type="text/javascript">
if ( ! window.console ) console = { log: function(){} };

var lastEnabledState = 0;

function GetTestMode()
{
	$.ajax({ url: "fppjson.php?command=getTestMode",
		async: false,
		dataType: 'json',
		success: function(data) {
			if (data.enabled)
			{
				$('#testModeEnabled').prop('checked', true);
				lastEnabledState = 1;

				$("#testModeCycleMSText").html(data.cycleMS);
				$("#testModeCycleMS").slider("value", data.cycleMS);

				if (data.mode == "SingleChase")
				{
					$("input[name=testModeMode][value=SingleChase]").prop('checked', true);
					$('#testModeChaseSize').val(data.chaseSize);
					$('#testModeColorSText').html(data.chaseValue);
					$("#testModeColorS").slider("value", data.chaseValue);
				}
				else if (data.mode == "RGBChase")
				{
					$("input[name=testModeMode][value=" + data.subMode + "]").prop('checked', true);
					if (data.subMode == "RGBChase-RGBCustom")
						$('#testModeRGBCustomPattern').val(data.colorPattern);
				}
				else if (data.mode == "RGBFill")
				{
					$("input[name=testModeMode][value=RGBFill]").prop('checked', true);
					$("#testModeColor1Text").html(data.color1);
					$("#testModeColor2Text").html(data.color2);
					$("#testModeColor3Text").html(data.color3);
					$("#testModeColor1").slider("value", data.color1);
					$("#testModeColor2").slider("value", data.color2);
					$("#testModeColor3").slider("value", data.color3);
				}
			}
			else
			{
				$('#testModeEnabled').prop('checked', false);
			}
		},
		failure: function(data) {
			$('#testModeEnabled').prop('checked', false);
		}
	});
}

function SetTestMode()
{
	var enabled = 0;
	var mode = "singleChase";
	var cycleMS = parseInt($('#testModeCycleMSText').html());
	var colorS = parseInt($('#testModeColorSText').html());
	var color1 = parseInt($('#testModeColor1Text').html());
	var color2 = parseInt($('#testModeColor2Text').html());
	var color3 = parseInt($('#testModeColor3Text').html());
	var startChannel = parseInt($('#testModeStartChannel').val());
	var endChannel = parseInt($('#testModeEndChannel').val());
	var chaseSize = parseInt($('#testModeChaseSize').val());
	var maxChannel = 524288;
	var channelSetType = "channelRange";

	if (startChannel < 1 || startChannel > maxChannel)
		startChannel = 1;

	if (endChannel < 1 || endChannel > maxChannel)
		endChannel = maxChannel;
	
	if (endChannel < startChannel)
		endChannel = startChannel;

	var selected = $("#testModeModeDiv input[type='radio']:checked");
	if (selected.length > 0) {
		mode = selected.val();
	}

	if ($('#testModeEnabled').is(':checked'))
		enabled = 1;

	if (enabled || lastEnabledState)
	{
		var data = {};
		var channelSet = "\"" + startChannel + "-" + endChannel + "\"";

		if (mode == "SingleChase")
		{
			data =
				{
					mode: "SingleChase",
					cycleMS: cycleMS,
					chaseSize: chaseSize,
					chaseValue: colorS
				};
		}
		else if (mode.substring(0,9) == "RGBChase-")
		{
			var colorPattern = "FF000000FF000000FF"; // R-G-B

			if (mode == "RGBChase-RGB")
			{
				colorPattern = "FF000000FF000000FF";
			}
			else if (mode == "RGBChase-RGBN")
			{
				colorPattern = "FF000000FF000000FF000000";
			}
			else if (mode == "RGBChase-RGBA")
			{
				colorPattern = "FF000000FF000000FFFFFFFF";
			}
			else if (mode == "RGBChase-RGBAN")
			{
				colorPattern = "FF000000FF000000FFFFFFFF000000";
			}
			else if (mode == "RGBChase-RGBCustom")
			{
				colorPattern = $('#testModeRGBCustomPattern').val();
			}

			data =
				{
					mode: "RGBChase",
					subMode: mode,
					cycleMS: cycleMS,
					colorPattern: colorPattern
				};
		}
		else if (mode == "SingleFill")
		{
			data =
				{
					mode: "RGBFill",
					color1: colorS,
					color2: colorS,
					color3: colorS
				};
		}
		else if (mode == "RGBFill")
		{
			data =
				{
					mode: "RGBFill",
					color1: color1,
					color2: color2,
					color3: color3
				};
		}

		data.enabled = enabled;
		data.channelSet = channelSet;
		data.channelSetType = channelSetType;


		var postData = "command=setTestMode&data=" + JSON.stringify(data);

		$.post("fppjson.php", postData).success(function(data) {
//			$.jGrowl("Test Mode Set");
		}).fail(function(data) {
			DialogError("Failed to set Test Mode", "Setup failed");
		});
	}

	lastEnabledState = enabled;
}

function DisableTestMode()
{
	$('#testModeEnabled').prop('checked', false);
	SetTestMode();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Sequence Testing Functions
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
function PlaySequence()
{
	var sequence = $('#selSequence').val();
	var startSecond = $('#startSecond').val();

	$.get("fppjson.php?command=startSequence&sequence=" + sequence + "&startSecond=" + startSecond
	).success(function() {
		$.jGrowl("Started sequence " + sequence);
		//$('#playSequence').hide();
		//$('#stopSequence').show();
	}).fail(function() {
		DialogError("Failed to start sequence", "Start failed");
	});
}

function StopSequence()
{
	$.get("fppjson.php?command=stopSequence"
	).success(function() {
		$.jGrowl("Stopped sequence");
		//$('#stopSequence').hide();
		//$('#playSequence').show();
	}).fail(function() {
		DialogError("Failed to stop sequence", "Stop failed");
	});
}

$(document).ready(function(){
	$("#tabs").tabs({cache: true, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });

	$('#testModeCycleMS').slider({
		min: 100,
		max: 5000,
		value: 1000,
		step: 100,
		slide: function( event, ui ) {
			testModeTimerInterval = ui.value;
			$('#testModeCycleMSText').html(testModeTimerInterval);
		},
		stop: function( event, ui ) {
			testModeTimerInterval = $('#testModeCycleMS').slider('value');
			$('#testModeCycleMSText').html(testModeTimerInterval);
			SetTestMode();
		}
		});

	$('#testModeColorS').slider({
		min: 0,
		max: 255,
		value: 255,
		step: 1,
		slide: function( event, ui ) {
			testModeColorS = ui.value;
			$('#testModeColorSText').html(testModeColorS);
		},
		stop: function( event, ui ) {
			testModeColorS = $('#testModeColorS').slider('value');
			$('#testModeColorSText').html(testModeColorS);
			SetTestMode();
		}
		});

	$('#testModeColor1').slider({
		min: 0,
		max: 255,
		value: 255,
		step: 1,
		slide: function( event, ui ) {
			testModeColor1 = ui.value;
			$('#testModeColor1Text').html(testModeColor1);
		},
		stop: function( event, ui ) {
			testModeColor1 = $('#testModeColor1').slider('value');
			$('#testModeColor1Text').html(testModeColor1);
			SetTestMode();
		}
		});

	$('#testModeColor2').slider({
		min: 0,
		max: 255,
		value: 255,
		step: 1,
		slide: function( event, ui ) {
			testModeColor2 = ui.value;
			$('#testModeColor2Text').html(testModeColor2);
		},
		stop: function( event, ui ) {
			testModeColor2 = $('#testModeColor2').slider('value');
			$('#testModeColor2Text').html(testModeColor2);
			SetTestMode();
		}
		});

	$('#testModeColor3').slider({
		min: 0,
		max: 255,
		value: 255,
		step: 1,
		slide: function( event, ui ) {
			testModeColor3 = ui.value;
			$('#testModeColor3Text').html(testModeColor3);
		},
		stop: function( event, ui ) {
			testModeColor3 = $('#testModeColor3').slider('value');
			$('#testModeColor3Text').html(testModeColor3);
			SetTestMode();
		}
		});

	GetTestMode();
});

</script>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div>
		<br>
		<div class='title'>Display Testing</div>
		<div id="tabs">
			<ul>
				<li><a href='#tab-channels'>Channel Testing</a></li>
				<li><a href='#tab-sequence'>Sequence</a></li>
			</ul>
		<div id='tab-channels'>
		<div>
			<fieldset class='fs'>
      <legend>Channel Output Testing</legend>
      <div>
				Enable Test Mode: <input type='checkbox' id='testModeEnabled' onClick='SetTestMode();'><br>
				<hr>
				<b>Channel Range to Test</b><br>
				<table border=0 cellspacing='2' cellpadding='2'>
				<tr><td colspan=2><input type='radio' name='testModeListType' value='Range' onChange='SetTestMode();' checked>Channel Range</td>
<!--
					<td width=40 rowspan=3>&nbsp;</td>
					<td colspan=2><input type='radio' name='testModeListType' value='Universe' onChange='SetTestMode();'>Universe</td>
					<td width=40 rowspan=3>&nbsp;</td>
					<td><input type='radio' name='testModeListType' value='Model' onChange='SetTestMode();'>Model</td>
-->
					</tr>
				<tr><td>Start Channel:</td>
						<td><input type='text' size='6' maxlength='6' value='1' id='testModeStartChannel' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'> (1-524288)</td>
<!--
						<td>Universe Size:</td>
						<td><input type='text' size=4 maxlength=4 value='512' id='testUniverseSize'></td>
						<td>Model Name:</td>
-->
						</tr>
				<tr><td>End Channel:</td>
						<td><input type='text' size='6' maxlength='6' value='524288' id='testModeEndChannel' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'> (1-524288)</td>
<!--
						<td>Universe #:</td>
						<td><input type='text' size=5 maxlength=5 value='1' id='testUniverseNumber'></td>
						<td>
							<select>
								<option>Mega Tree</option>
								<option>Matrix #1</option>
								<option>Matrix #2</option>
							</select>
							</td>
-->
						</tr>
				</table>
				<br>
				<span style='float: left'>Update Interval: </span><span id="testModeCycleMS"></span> <span style='float: left' id='testModeCycleMSText'>1000</span><span style='float: left'> ms</span></br>
				<hr>
				<div id='testModeModeDiv'>
				<b>Test Patterns</b><br><br>
				<table border=0 cellpadding=0 cellspacing=0>
				<tr><td colspan=3><b>Single Channel Patterns:</b></td></tr>
				<tr><td colspan=3><span style='float: left'><b>Test Value: </b></span><span id="testModeColorS"></span> <span style='float: left' id='testModeColorSText'>255</span><span style='float: left'></span></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='SingleChase' checked onChange='SetTestMode();'></td><td><b>Chase:</b></td></tr>
				<tr><td></td><td>Chase Size: <select id='testModeChaseSize' onChange='SetTestMode();'>
						<option value='2'>2</option>
						<option value='3'>3</option>
						<option value='4'>4</option>
						<option value='5'>5</option>
						<option value='6'>6</option>
					</select></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='SingleFill' onChange='SetTestMode();'></td><td><b>Fill</b></td></tr>
				<tr><td>&nbsp;</td></tr>
				<tr><td colspan=3><b>RGB Patterns:</b></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='RGBChase-RGB' onChange='SetTestMode();'></td><td><b>Chase: A-B-C</b></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='RGBChase-RGBA' onChange='SetTestMode();'></td><td><b>Chase: A-B-C-All</b></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='RGBChase-RGBN' onChange='SetTestMode();'></td><td><b>Chase: A-B-C-None</b></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='RGBChase-RGBAN' onChange='SetTestMode();'></td><td><b>Chase: A-B-C-All-None</b></td></tr>
				<tr><td><input type='radio' name='testModeMode' value='RGBChase-RGBCustom' onChange='SetTestMode();'></td><td><b>Chase: Custom Pattern: </b> <input id='testModeRGBCustomPattern' size='36' maxlength='72' value='FF000000FF000000FF' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'> (6 hex digits per RGB triplet)</td></tr>
				<tr><td><input type='radio' name='testModeMode' value='RGBFill' onChange='SetTestMode();'></td><td><b>Fill:</b></td></tr>
				<tr><td>&nbsp;</td><td>
					<table border=0 cellspacing=10 cellpadding=0>
						<tr><td><span style='float: left'>A: </span><span id="testModeColor1"></span> <span style='float: left' id='testModeColor1Text'>255</span><span style='float: left'></span></td></tr>
						<tr><td><span style='float: left'>B: </span><span id="testModeColor2"></span> <span style='float: left' id='testModeColor2Text'>255</span><span style='float: left'></span></td></tr>
						<tr><td><span style='float: left'>C: </span><span id="testModeColor3"></span> <span style='float: left' id='testModeColor3Text'>255</span><span style='float: left'></span></td></tr>
					</table>
					</td></tr>
				</table>
				</div>
			</div>
			</fieldset>
		</div>
		</div>
			<div id='tab-sequence'>
			<div>
    <fieldset class='fs'>
			<legend> Sequence Testing </legend>
      <div>
				<table border='0' cellspacing='3'>
					<tr><td>Sequence:</td>
							<td><?php PrintSequenceOptions();?></td></tr>
					<tr><td>Start Time:</td>
							<td><input type='text' size='4' maxlength='4' value='0' id='startSecond'> (Seconds from beginning of sequence)</td></tr>

					<tr><td><input type='button' value='Play' onClick='PlaySequence();' id='playSequence'><input type='button' value='Stop' onClick='StopSequence();' id='stopSequence'></td>
							<td>Play/stop the selected sequence</td></tr>
					<tr><td><input type='button' value='Pause/UnPause' onClick='ToggleSequencePause();'></td>
							<td>Pause a running sequence or UnPause a paused sequence</td></tr>
					<tr><td><input type='button' value='Step' onClick='SingleStepSequence();'></td>
							<td>Single-step a paused sequence one frame</td></tr>
					<tr><td><input type='button' value='Step Back' onClick='SingleStepSequenceBack();'></td>
							<td>Single-step a paused sequence backwards one frame</td></tr>
				</table>
				<br>
				<b>Sequence Testing Limitations:</b>
				<ol>
					<li>This page is for testing sequences, it does not test audio or video or synchronization of a sequence with any media file.  It does test Master/Remote sequence synchronization.</li>
					<li>The Sequence Testing functionality currently only works when FPP is in an idle state and no playlists are playing.  If a playlist starts while testing a sequence, the sequence being tested will be stopped automatically.</li>
				</ol>
      </div>
			</fieldset>
		</div>
		</div>
  </div>
  </div>

</div>
  <?php include 'common/footer.inc'; ?>
</body>
</html>
