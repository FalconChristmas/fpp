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

?>

<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
<style>

#rgbCycleSpeed {
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
<body>

<script type="text/javascript">
var wsIsOpen = 0;
var ws;
var dataIsPending = 0;
var pendingData;

if ( ! window.console ) console = { log: function(){} };

function SendWSCommand(data)
{
	if (!wsIsOpen)
	{
		dataIsPending = 1;
		pendingData = data;

		ws = new WebSocket("ws://<? echo $_SERVER['HTTP_HOST']; ?>:32321/echo");
		ws.onopen = function()
		{
			wsIsOpen = 1;
			if (dataIsPending)
			{
				dataIsPending = 0;
				ws.send(JSON.stringify(pendingData));
			}
		}
		ws.onmessage = function(evt)
		{
			var data = JSON.parse(evt.data);
			if (data.Command == "GetTestMode") {
				var parsedData = JSON.parse(evt.data)
				if (parsedData.State)
					$('#testMode').prop('checked', true);
				else
					$('#testMode').prop('checked', false);
			}
		},
     	ws.onclose = function()
		{ 
		 	wsIsOpen = 0;
		};
	} else {
		ws.send(JSON.stringify(data));
	}
}

var rgbCycleActive = 0;
var rgbCycleColors = Array();
var rgbCycleTimerInterval = 1000; // In ms
var rgbCycleCurrentColor = 0;

function rgbCycleCallback() {
	if (!rgbCycleActive)
		return;

	if (rgbCycleCurrentColor >= rgbCycleColors.length)
		rgbCycleCurrentColor = 0;

	var colors = rgbCycleColors[rgbCycleCurrentColor].split(',');

	setTestModeColor(colors[0], colors[1], colors[2]);

	rgbCycleCurrentColor++;
	if (rgbCycleCurrentColor >= rgbCycleColors.length)
		rgbCycleCurrentColor = 0;

	if (rgbCycleActive)
		setTimeout(function(){ rgbCycleCallback()}, rgbCycleTimerInterval);
}

function rgbColorsChanged() {
	var selectedVal = "";
	var selected = $("#rgbCycleOptionDiv input[type='radio']:checked");
	if (selected.length > 0) {
		selectedVal = selected.val();

		if (selectedVal == "RGB")
		{
			rgbCycleColors = Array(
				"255,0,0", "0,255,0", "0,0,255"
				);
		}
		else if (selectedVal == "RGBW")
		{
			rgbCycleColors = Array(
				"255,0,0", "0,255,0", "0,0,255", "255,255,255"
				);
		}
		else if (selectedVal == "RGBN")
		{
			rgbCycleColors = Array(
				"255,0,0", "0,255,0", "0,0,255", "0,0,0"
				);
		}
		else if (selectedVal == "RGBWN")
		{
			rgbCycleColors = Array(
				"255,0,0", "0,255,0", "0,0,255", "255,255,255", "0,0,0"
				);
		}

		return 1;
	}

	return 0;
}

function rgbCycleChanged() {
	if (!$('#rgbCycle').is(':checked'))
	{
		if (rgbCycleActive)
		{
			setTestModeColor(0, 0, 0);
			rgbCycleActive = 0;
		}

		return;
	}

	if (rgbColorsChanged())
	{
		rgbCycleActive = 1;
		rgbCycleCurrentColor = 0;

		setTimeout(function(){ rgbCycleCallback()}, rgbCycleTimerInterval);
	}
	else
	{
		rgbCycleActive = 0;
	}
}

function getTestMode() {
	SendWSCommand({ Command: "GetTestMode" });
}

function testModeOn() {
	SendWSCommand({ Command: "SetTestMode", State: 1 });
}

function testModeOff() {
	SendWSCommand({ Command: "SetTestMode", State: 0 });
}

function setSingleValue(value) {
	var channelNumber = $('#channelNumber').val();

	SendWSCommand({ Command: "SetChannel", Channel: channelNumber, Value: value });
}

function setSingleChannel() {
	var channelNumber = parseInt($('#channelNumber').val()) - 1;
	var channelValue = $('#channelValue').val();

	SendWSCommand({ Command: "SetChannel", Channel: channelNumber, Value: channelValue });
}

function setTestModeColor(r, g, b) {
	$('#rgbCustomR').slider('value', r);
	$('#rgbCustomRText').html(r);
	$('#rgbCustomG').slider('value', g);
	$('#rgbCustomGText').html(g);
	$('#rgbCustomB').slider('value', b);
	$('#rgbCustomBText').html(b);

	SendWSCommand( { Command: "SetTestModeColor",
		RGB: [ r, g, b ] } );
}

function setTestModeCustomColor() {
	var r = $('#rgbCustomR').slider('value');
	var g = $('#rgbCustomG').slider('value');
	var b = $('#rgbCustomB').slider('value');

	setTestModeColor(r, g, b);
}

function TestModeChanged() {
	if ($('#testMode').is(':checked'))
	{
		testModeOn();
	}
	else
	{
		testModeOff();
		setTestModeColor(0, 0, 0);

		// RGB Cycle
		rgbCycleActive = 0;
		$('#rgbCycle').prop('checked', false);
	}
}

</script>

<script>

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
	getTestMode();

	$('#rgbCycleSpeed').slider({
		min: 1000,
		max: 5000,
		value: 1000,
		step: 500,
		stop: function( event, ui ) {
			rgbCycleTimerInterval = $('#rgbCycleSpeed').slider('value');
			$('#rgbCycleSpeedText').html(rgbCycleTimerInterval);
		}
		});

	$('#rgbCustomR').slider({
		min: 0,
		max: 255,
		value: 0,
		step: 1,
		stop: function( event, ui ) {
			setTestModeCustomColor();
			$('#rgbCustomRText').html(ui.value);
		}
		});

	$('#rgbCustomG').slider({
		min: 0,
		max: 255,
		value: 0,
		step: 1,
		stop: function( event, ui ) {
			setTestModeCustomColor();
			$('#rgbCustomGText').html(ui.value);
		}
		});

	$('#rgbCustomB').slider({
		min: 0,
		max: 255,
		value: 0,
		step: 1,
		stop: function( event, ui ) {
			setTestModeCustomColor();
			$('#rgbCustomBText').html(ui.value);
		}
		});
});

</script>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div>
		<br>
		<div class='title'>Display Testing</div>
		<div id="tabs">
			<ul>
				<li><a href='#tab-channels'>Channel Outputs</a></li>
				<li><a href='#tab-sequence'>Sequence</a></li>
			</ul>
		<div id='tab-channels'>
		<div>
			<fieldset class='fs'>
      <legend>Channel Output Testing</legend>
      <div>
				Enable Test Mode: <input type='checkbox' id='testMode' onClick='TestModeChanged();'><br>
				<hr>
				<b>Single Channel Test</b><br>
				Channel Number: <input type='text' size='6' maxlength='6' value='0' id='channelNumber'> (1-131072)<br>
				Brightness: <input type='text' size='3' maxlength='3' value='0' id='channelValue'> (0-255)<br>
				<input type='button' value='Set' onClick='setSingleChannel();'>
				<hr>
				<b>Test All Channels</b><br>
				Preset Colors:
				<input type='button' value='Red' onClick='setTestModeColor(255, 0, 0);'>
				<input type='button' value='Green' onClick='setTestModeColor(0, 255, 0);'>
				<input type='button' value='Blue' onClick='setTestModeColor(0, 0, 255);'>
				<input type='button' value='White' onClick='setTestModeColor(255, 255, 255);'>
				<input type='button' value='Off' onClick='setTestModeColor(0, 0, 0);'>
				<br>
				Custom Color:<br>
				<span id="rgbCustomR" class='rgbCustomColor'></span><span style='float: left'>R: </span><span style='float: left' id='rgbCustomRText'>0</span><br>
				<span id="rgbCustomG" class='rgbCustomColor'></span><span style='float: left'>G: </span><span style='float: left' id='rgbCustomGText'>0</span><br>
				<span id="rgbCustomB" class='rgbCustomColor'></span><span style='float: left'>B: </span><span style='float: left' id='rgbCustomBText'>0</span><br>
				<br>
				<br>
				RGB Cycle: <input type='checkbox' id='rgbCycle' onClick='rgbCycleChanged();'><br>
				<span style='float: left'>Interval: </span><span id="rgbCycleSpeed"></span> <span style='float: left' id='rgbCycleSpeedText'>1000</span><span style='float: left'> ms</span></br>
				<div id='rgbCycleOptionDiv'>
				<input type='radio' name='rgbCycleOption' value='RGB' checked onChange='rgbColorsChanged();'> R-G-B<br>
				<input type='radio' name='rgbCycleOption' value='RGBW' onChange='rgbColorsChanged();'> R-G-B-All<br>
				<input type='radio' name='rgbCycleOption' value='RGBN' onChange='rgbColorsChanged();'> R-G-B-None<br>
				<input type='radio' name='rgbCycleOption' value='RGBWN' onChange='rgbColorsChanged();'> R-G-B-All-None<br>
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
							<td>Single-step a running sequence one frame</td></tr>
					<tr><td><input type='button' value='Step Back' onClick='SingleStepSequenceBack();'></td>
							<td>Single-step a running sequence <b>backwards</b> one frame</td></tr>
				</table>
				<br>
				<b>Note:</b> The Sequence Testing functionality currently only works when FPP is in an idle state and no playlists are playing.  If a playlist starts while testing a sequence, the sequence being tested will be stopped automatically.<br>
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
