<!DOCTYPE html>
<html>
<?php
require_once('config.php');
require_once('common.php');
require_once('fppdefines.php');

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

$testStartChannel = 1;
$testEndChannel = FPPD_MAX_CHANNELS;
if (file_exists("/home/fpp/media/fpp-info.json")) {
    $content = file_get_contents("/home/fpp/media/fpp-info.json");
    $json = json_decode($content, true);
    $channelRanges = $json['channelRanges'];
    if ($channelRanges != "") {
        $testStartChannel = FPPD_MAX_CHANNELS;
        $testEndChannel = 1;
        $ranges = explode(',', $channelRanges);
        foreach ($ranges as $range) {
            $minmax = explode('-', $range);

            if ($minmax[0] < $testStartChannel) {
                $testStartChannel = $minmax[0] + 1;
            }
            if ($minmax[1] > $testEndChannel) {
                $testEndChannel = $minmax[1] + 1;
            }
        }

        if ($testEndChannel < $testStartChannel) {
            $tmp = $testEndChannel;
            $testEndChannel = $testStartChannel;
            $testStartChannel = $tmp;
        }
    }
}
?>

<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" type="text/css" href="jquery/colpick/css/colpick.css">
<link rel="stylesheet" type="text/css" href="css/jquery.colpick.css">
<script type="text/javascript" src="jquery/colpick/js/colpick.js"></script>
<title><? echo $pageTitle; ?></title>

</head>
<body onunload='DisableTestMode();'>

<script type="text/javascript">
if ( ! window.console ) console = { log: function(){} };

var modelInfos = [];
var lastEnabledState = 0;

function StringsChanged()
{
    var id = parseInt($('#modelName').val());

    var startChan = modelInfos[id].StartChannel + (modelInfos[id].ChannelsPerString * (parseInt($('#startString').val()) - 1));
    var endChan = modelInfos[id].StartChannel + (modelInfos[id].ChannelsPerString * (parseInt($('#endString').val())) - 1);

    $('#testModeStartChannel').val(startChan);
    $('#testModeEndChannel').val(endChan);

    SetTestMode();
}

function AdjustStartString(delta = 1)
{
    var id = parseInt($('#modelName').val());

    var start = parseInt($('#startString').val());
    var end = parseInt($('#endString').val());

    start += delta;

    if (start > modelInfos[id].StringCount)
        start = modelInfos[id].StringCount;

    if (start < 1)
        start = 1;

    if (end < start) {
        end = start;
        $('#endString').val(end);
    }

    $('#startString').val(start);

    StringsChanged();
}

function AdjustEndString(delta = 1)
{
    var id = parseInt($('#modelName').val());

    var start = parseInt($('#startString').val());
    var end = parseInt($('#endString').val());

    end += delta;

    if (end > modelInfos[id].StringCount)
        end = modelInfos[id].StringCount;

    if (end < 1)
        end = 1;

    if (end < start) {
        start = end;
        $('#startString').val(start);
    }

    $('#endString').val(end);

    StringsChanged();
}

function UpdateStartEndFromModel()
{
    var val = $('#modelName').val();
    if (val.indexOf(',') != -1) {
        var parts = val.split(',');
        $('#testModeStartChannel').val(parseInt(parts[0]));
        $('#testModeEndChannel').val(parseInt(parts[1]));
        $('.stringRow').hide();
        $('#channelIncrement').val(3);
        SetButtonIncrements();
    } else {
        var id = parseInt(val);
        $('#testModeStartChannel').val(modelInfos[id].StartChannel);
        $('#testModeEndChannel').val(modelInfos[id].EndChannel);

        if (modelInfos[id].StringCount > 1) {
            $('#startString').attr('max', modelInfos[id].StringCount);
            $('#startString').val(1);
            $('#endString').attr('max', modelInfos[id].StringCount);
            $('#endString').val(modelInfos[id].StringCount);
            $('#channelIncrement').val(modelInfos[id].ChannelsPerString);
            $('.stringRow').show();
        } else {
            $('#channelIncrement').val(3);
            $('.stringRow').hide();
        }
        SetButtonIncrements();
    }

	if (lastEnabledState)
	{
		var data = {};

		data.enabled = 0;

		var postData = "command=setTestMode&data=" + JSON.stringify(data);

		$.post("fppjson.php", postData).done(function(data) {
			SetTestMode();
//			$.jGrowl("Test Mode Disabled");
		}).fail(function(data) {
			DialogError("Failed to set Test Mode", "Setup failed");
		});
	}
	else
	{
		SetTestMode();
	}
}

function GetTestMode()
{
	$.ajax({ url: "api/testmode",
		async: false,
		dataType: 'json',
		success: function(data) {
			if (data.enabled)
			{
				$('#testModeEnabled').prop('checked', true);
				lastEnabledState = 1;

                if (data.hasOwnProperty('cycleMS')) {
                    $("#testModeCycleMSText").html(data.cycleMS);
                    $("#testModeCycleMS").val(data.cycleMS);
                } else {
                    $("#testModeCycleMSText").html(1000);
                    $("#testModeCycleMS").val(1000);
                }
				if (data.mode == "SingleChase")
				{
					$("input[name=testModeMode][value=SingleChase]").prop('checked', true);
					$('#testModeChaseSize').val(data.chaseSize);
					$('#testModeColorSText').html(data.chaseValue);
					$("#testModeColorS").val(data.chaseValue);
				}
				else if (data.mode == "RGBChase")
				{
					$("input[name=testModeMode][value=" + data.subMode + "]").prop('checked', true);
					if (data.subMode == "RGBChase-RGBCustom")
						$('#testModeRGBCustomPattern').val(data.colorPattern);
				}
				else if (data.mode == "RGBCycle")
				{
					$("input[name=testModeMode][value=" + data.subMode + "]").prop('checked', true);
					if (data.subMode == "RGBCycle-RGBCustom")
						$('#testModeRGBCycleCustomPattern').val(data.colorPattern);
				}
				else if (data.mode == "RGBFill")
				{
					$("input[name=testModeMode][value=RGBFill]").prop('checked', true);
					$("#testModeColorRText").html(data.color1);
					$("#testModeColorGText").html(data.color2);
					$("#testModeColorBText").html(data.color3);
					$("#testModeColorR").val(data.color1);
					$("#testModeColorG").val(data.color2);
					$("#testModeColorB").val(data.color3);
					var rgb = {
						r:data.color1,
						g:data.color2,
						b:data.color3
					};
					$('.color-box').colpickSetColor(rgb).css('background-color', $.colpick.rgbToHex(rgb));	
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
	var colorR = parseInt($('#testModeColorRText').html());
	var colorG = parseInt($('#testModeColorGText').html());
	var colorB = parseInt($('#testModeColorBText').html());
	var color1;
	var color2;
	var color3;
	var strR = "FF0000";
	var strG = "00FF00";
	var strB = "0000FF";
	var startChannel = parseInt($('#testModeStartChannel').val());
	var endChannel = parseInt($('#testModeEndChannel').val());
	var chaseSize = parseInt($('#testModeChaseSize').val());
	var maxChannel = 8*1024*1024;
	var channelSetType = "channelRange";
	var colorOrder = $('#colorOrder').val();

	if (colorOrder == "RGB") {
		color1 = colorR;
		color2 = colorG;
		color3 = colorB;
		strR = "FF0000";
		strG = "00FF00";
		strB = "0000FF";
	} else if (colorOrder == "RBG") {
		color1 = colorR;
		color3 = colorG;
		color2 = colorB;
		strR = "FF0000";
		strG = "0000FF";
		strB = "00FF00";
	} else if (colorOrder == "GRB") {
		color2 = colorR;
		color1 = colorG;
		color3 = colorB;
		strR = "00FF00";
		strG = "FF0000";
		strB = "0000FF";
	} else if (colorOrder == "GBR") {
		color3 = colorR;
		color1 = colorG;
		color2 = colorB;
		strR = "0000FF";
		strG = "FF0000";
		strB = "00FF00";
	} else if (colorOrder == "BRG") {
		color2 = colorR;
		color3 = colorG;
		color1 = colorB;
		strR = "00FF00";
		strG = "0000FF";
		strB = "FF0000";
	} else if (colorOrder == "BGR") {
		color3 = colorR;
		color2 = colorG;
		color1 = colorB;
		strR = "0000FF";
		strG = "00FF00";
		strB = "FF0000";
	}

	if (startChannel < 1 || startChannel > maxChannel || isNaN(startChannel))
		startChannel = 1;

	if (endChannel < 1 || endChannel > maxChannel || isNaN(endChannel))
		endChannel = maxChannel;
	
	if (endChannel < startChannel)
		endChannel = startChannel;

	var selected = $("#tab-channels input[type='radio']:checked");
	if (selected.length > 0) {
		mode = selected.val();
	}

	if ($('#testModeEnabled').is(':checked'))
		enabled = 1;

	if (enabled || lastEnabledState)
	{
		var data = {};
		var channelSet = "" + startChannel + "-" + endChannel;

		if (mode == "SingleChase") {
			data =
				{
					mode: "SingleChase",
					cycleMS: cycleMS,
					chaseSize: chaseSize,
					chaseValue: colorS
				};
		} else if (mode.substring(0,9) == "RGBChase-") {
			var colorPattern = strR + strG + strB;
			if (mode == "RGBChase-RGB") {
				colorPattern = strR + strG + strB;
			} else if (mode == "RGBChase-RGBN") {
				colorPattern = strR + strG + strB + "000000";
			} else if (mode == "RGBChase-RGBA") {
				colorPattern = strR + strG + strB + "FFFFFF";
			} else if (mode == "RGBChase-RGBAN") {
				colorPattern = strR + strG + strB + "FFFFFF000000";
			} else if (mode == "RGBChase-RGBCustom") {
				colorPattern = $('#testModeRGBCustomPattern').val();
			}
			data =
				{
					mode: "RGBChase",
					subMode: mode,
					cycleMS: cycleMS,
					colorPattern: colorPattern
				};
		} else if (mode.substring(0,9) == "RGBCycle-") {
			var colorPattern = strR + strG + strB;
			if (mode == "RGBCycle-RGB") {
				colorPattern = strR + strG + strB;
			} else if (mode == "RGBCycle-RGBN") {
				colorPattern = strR + strG + strB + "000000";
			} else if (mode == "RGBCycle-RGBA") {
				colorPattern = strR + strG + strB + "FFFFFF";
			} else if (mode == "RGBCycle-RGBAN") {
				colorPattern = strR + strG + strB + "FFFFFF000000";
			} else if (mode == "RGBCycle-RGBCustom") {
				colorPattern = $('#testModeRGBCycleCustomPattern').val();
			}
			data =
				{
					mode: "RGBCycle",
					subMode: mode,
					cycleMS: cycleMS,
					colorPattern: colorPattern
				};
		} else if (mode == "SingleFill") {
			data =
				{
					mode: "RGBFill",
					color1: colorS,
					color2: colorS,
					color3: colorS
				};
		} else if (mode == "RGBFill") {
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

		var postData = JSON.stringify(data);
		console.log(postData);
		$.post("api/testmode", postData).done(function(data) {
			//$.jGrowl("Test Mode Set");
			//console.log(data);
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

function SetButtonIncrements()
{
    var delta = $('#channelIncrement').val();

    $('#incStartButton').val('+' + delta);
    $('#decStartButton').val('-' + delta);
    $('#incBothButton').val('+' + delta);
    $('#decBothButton').val('-' + delta);
    $('#incEndButton').val('+' + delta);
    $('#decEndButton').val('-' + delta);
}

function adjustBothChannels(mult = 1)
{
    if (mult > 0) {
        adjustEndChannel(mult);
        adjustStartChannel(mult);
    } else {
        adjustStartChannel(mult);
        adjustEndChannel(mult);
    }
}

function adjustStartChannel(mult = 1)
{
    var start = parseInt($('#testModeStartChannel').val());
    var end = parseInt($('#testModeEndChannel').val());

    var delta = parseInt($('#channelIncrement').val()) * mult;

    $('.stringRow').hide();

    start += delta;

    if (start > <? echo FPPD_MAX_CHANNELS; ?>)
        start = <? echo FPPD_MAX_CHANNELS; ?>;
    else if (start < 1)
        start = 1;

    if (end < start) {
        end = start;
        $('#testModeEndChannel').val(end);
    }

    $('#testModeStartChannel').val(start);

    SetTestMode();
}

function adjustEndChannel(mult = 1)
{
    var start = parseInt($('#testModeStartChannel').val());
    var end = parseInt($('#testModeEndChannel').val());

    var delta = parseInt($('#channelIncrement').val()) * mult;

    $('.stringRow').hide();

    end += delta;

    if (end > <? echo FPPD_MAX_CHANNELS; ?>)
        end = <? echo FPPD_MAX_CHANNELS; ?>;
    else if (end < 1)
        end = 1;

    if (end < start)
        end = start;

    $('#testModeEndChannel').val(end);

    SetTestMode();
}

function dec2hex(i) {
	return (i+0x100).toString(16).substr(-2).toUpperCase();
}

function AppendFillToCustom()
{
	var colorR = dec2hex(parseInt($('#testModeColorRText').html()));
	var colorG = dec2hex(parseInt($('#testModeColorGText').html()));
	var colorB = dec2hex(parseInt($('#testModeColorBText').html()));

	var newTriplet = colorR + colorG + colorB;

	var currentValue = $('#testModeRGBCustomPattern').val();
	$('#testModeRGBCustomPattern').val(currentValue + newTriplet);

    currentValue = $('#testModeRGBCycleCustomPattern').val();
    $('#testModeRGBCycleCustomPattern').val(currentValue + newTriplet);

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

	$.get("api/sequence/" + sequence + "/start/" + startSecond
	).done(function() {
		$.jGrowl("Started sequence " + sequence,{themeState:'success'});
	}).fail(function() {
		DialogError("Failed to start sequence", "Start failed");
	});
}

function StopSequence()
{
	var url = "api/sequence/current/stop";
	var sequence = $('#selSequence').val();

	$.get(url
	).done(function() {
		$.jGrowl("Stop sequence " + sequence,{themeState:'danger'});
	}).fail(function() {
		DialogError("Failed to stop sequence", "Stop failed");
	});

}
function UpdateTestModeFillColors(){
	var rgb = {
		r:parseInt($('#testModeColorR').val()), 
		g:parseInt($('#testModeColorG').val()), 
		b:parseInt($('#testModeColorB').val())
	}
	$('#testModeColorRText').html(rgb.r);
	$('#testModeColorGText').html(rgb.g);
	$('#testModeColorBText').html(rgb.b);
	$('.color-box').colpickSetColor($.colpick.rgbToHex(rgb));
}
$(document).ready(function(){

    $.ajax({
        url: 'api/models',
        type: 'GET',
        async: true,
        dataType: 'json',
        success: function (data) {
            modelInfos = data;

            for (var i = 0; i < modelInfos.length; i++) {
                modelInfos[i].EndChannel = modelInfos[i].StartChannel + modelInfos[i].ChannelCount - 1;
                modelInfos[i].ChannelsPerString = parseInt(modelInfos[i].ChannelCount / modelInfos[i].StringCount);
                var option = "<option value='" + i + "'>" + modelInfos[i].Name + "</option>\n";
                $('#modelName').append(option);
            }
        },
        error: function () {
            $.jGrowl('Error: Unable to get list of models', { themeState: 'danger' });
        }
    });

	$('#testModeCycleMS').on('input',function(){
		testModeTimerInterval = $('#testModeCycleMS').val();
		$('#testModeCycleMSText').html(testModeTimerInterval);
	}).on('change',function(){
		testModeTimerInterval = $('#testModeCycleMS').val();
		$('#testModeCycleMSText').html(testModeTimerInterval);
		SetTestMode();
	})
	// $('#testModeCycleMS').slider({
	// 	min: 100,
	// 	max: 5000,
	// 	value: 1000,
	// 	step: 100,
	// 	slide: function( event, ui ) {
	// 		testModeTimerInterval = $('#testModeCycleMS').val();
	// 		$('#testModeCycleMSText').html(testModeTimerInterval);
	// 	},
	// 	stop: function( event, ui ) {
	// 		testModeTimerInterval = $('#testModeCycleMS').val();
	// 		$('#testModeCycleMSText').html(testModeTimerInterval);
	// 		SetTestMode();
	// 	}
	// 	});
	$('#testModeColorS').on('input',function(){
		testModeColorS =$('#testModeColorS').val();
			$('#testModeColorSText').html(testModeColorS);
	}).on('change',function(){
			testModeColorS = $('#testModeColorS').val();
			$('#testModeColorSText').html(testModeColorS);
			SetTestMode();
	})
	// $('#testModeColorS').slider({
	// 	min: 0,
	// 	max: 255,
	// 	value: 255,
	// 	step: 1,
	// 	slide: function( event, ui ) {
	// 		testModeColorS = ui.value;
	// 		$('#testModeColorSText').html(testModeColorS);
	// 	},
	// 	stop: function( event, ui ) {
	// 		testModeColorS = $('#testModeColorS').slider('value');
	// 		$('#testModeColorSText').html(testModeColorS);
	// 		SetTestMode();
	// 	}
	// 	});
	$('#testModeColorR').on('input',function(){
		UpdateTestModeFillColors();
	}).on('change',function(){
		UpdateTestModeFillColors();
		SetTestMode();
	})

	// $('#testModeColorR').slider({
	// 	min: 0,
	// 	max: 255,
	// 	value: 255,
	// 	step: 1,
	// 	slide: function( event, ui ) {
	// 		testModeColorR = ui.value;
	// 		$('#testModeColorRText').html(testModeColorR);
	// 	},
	// 	stop: function( event, ui ) {
	// 		testModeColorR = $('#testModeColorR').slider('value');
	// 		$('#testModeColorRText').html(testModeColorR);
	// 		$('.color-box').colpickSetColor($.colpick.rgbToHex({r:testModeColorR, g:$('#testModeColorG').slider('value'), b:$('#testModeColorB').slider('value')}));
	// 		SetTestMode();
	// 	}
	// 	});

	$('#testModeColorG').on('input',function(){
		UpdateTestModeFillColors();
	}).on('change',function(){
		UpdateTestModeFillColors();
		SetTestMode();
	})
	// $('#testModeColorG').slider({
	// 	min: 0,
	// 	max: 255,
	// 	value: 255,
	// 	step: 1,
	// 	slide: function( event, ui ) {
	// 		testModeColorG = ui.value;
	// 		$('#testModeColorGText').html(testModeColorG);
	// 		$('.color-box').colpickSetColor($.colpick.rgbToHex({r:$('#testModeColorR').slider('value'), g:testModeColorG, b:$('#testModeColorB').slider('value')}));
	// 	},
	// 	stop: function( event, ui ) {
	// 		testModeColorG = $('#testModeColorG').slider('value');
	// 		$('#testModeColorGText').html(testModeColorG);
	// 		$('.color-box').colpickSetColor($.colpick.rgbToHex({r:$('#testModeColorR').slider('value'), g:testModeColorG, b:$('#testModeColorB').slider('value')}));
	// 		SetTestMode();
	// 	}
	// 	});
	$('#testModeColorB').on('input',function(){
		UpdateTestModeFillColors();
	}).on('change',function(){
		UpdateTestModeFillColors();
		SetTestMode();
	});
	
	$('.color-box').colpick({
		layout:'rgbhex',
		color:'ff00ff',
		submit:false,
		onChange:function(hsb,hex,rgb,el,bySetColor) {
			$(el).css('background-color', '#'+hex);
			if(!bySetColor) {
				// Set each of the sliders and text to the new value
				testModeColorR = rgb.r;
				$('#testModeColorR').val(testModeColorR);
				$('#testModeColorRText').html(testModeColorR);
				testModeColorG = rgb.g;
				$('#testModeColorG').val(testModeColorG);
				$('#testModeColorGText').html(testModeColorG);
				testModeColorB = rgb.b;
				$('#testModeColorB').val(testModeColorB);
				$('#testModeColorBText').html(testModeColorB);
				SetTestMode();
			}
		}
	}).keyup(function(){
		$(this).colpickSetColor(this.value);
	})
	.css('background-color', '#ff00ff');	

	GetTestMode();
});

</script>



<div id="bodyWrapper">
  <?php 
  $activeParentMenuItem = 'status'; 
  include 'menu.inc'; ?>
  <div class="mainContainer">
	  <h2 class="title">Display Testing</h2>
	  <div class="pageContent">
			<div id='channelTester'>

		<div id="tabs">

		<ul class="nav nav-pills pageContent-tabs" role="tablist">
			<li class="nav-item">
				<a class="nav-link active" id="tab-channels-tab" data-toggle="tab" href="#tab-channels" role="tab" aria-controls="tab-channels" aria-selected="true">
					Channel Testing
				</a>
			</li>
			<li class="nav-item">
				<a class="nav-link" id="tab-sequence-tab" data-toggle="tab" href="#tab-sequence" role="tab" aria-controls="tab-sequence" aria-selected="true">
				Sequence
				</a>
			</li>
		</ul>

	<div class="tab-content">
		<div id='tab-channels' class="tab-pane fade show active" role="tabpanel" aria-labelledby="interface-settings-tab">

							
				<div class="row">
					<div class="col-md-3">
						<div class="backdrop-dark">
							<label for="testModeEnabled" class="mb-0 d-block">
								<b>Enable Test Mode:</b>
								<input type='checkbox' class="ml-1" id='testModeEnabled' onClick='SetTestMode();'>
							</label> 
						</div>
						<div class="backdrop-dark mt-3">

							<div class="form-group">
								<div><b>Model Name:</b></div>
								<div>
									<select onChange='UpdateStartEndFromModel();' id='modelName'>
													<option value='1,<?=$testEndChannel?>'>-- All Channels --</option>
							</select>
							</div>
							</div>

							<div class="mb-1"><b>Channel Range to Test</b><small  class="form-text text-muted">(1-<? echo FPPD_MAX_CHANNELS; ?>)  </small></div>
							
							<div class="row">
								<div class="col-6 form-group">
									<label for="testModeStartChannel">Start Channel:</label>
									<input class="form-control" type='number' min='1' max='<? echo FPPD_MAX_CHANNELS; ?>' value='<?=$testStartChannel ?>' id='testModeStartChannel' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'>
								</div>
								<div class="col-6 form-group">
									<label for="testModeEndChannel">End Channel:</label>
									<input class="form-control" type='number' min='1' max='<? echo FPPD_MAX_CHANNELS; ?>' value='<?=$testEndChannel ?>' id='testModeEndChannel' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'>
								</div>
							</div>

							<div class="mb-1"><b>Adjust Start/End Channels</b></div>
							<div class='row'>
								<div class="col-6 form-group">
									<label for='channelIncrement'>Increment:</label>
								</div>
								<div class="col-6 form-group">
									<input class="form-control" type='number' min='1' max='<? echo FPPD_MAX_CHANNELS; ?>' value='3' id='channelIncrement' onChange='SetButtonIncrements();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'>
								</div>
							</div>
					
							<div class='row'>
								<div class="col-6 form-group">
									<label>Start Channel:</label>
								</div>
								<div class="col-6 form-group">
								    <input type='button' class='buttons' value='-3' id='decStartButton' onClick='adjustStartChannel(-1);'>
									<input type='button' class='buttons' value='+3' id='incStartButton' onClick='adjustStartChannel(1);'>
								</div>
							</div>
							<div class='row'>
								<div class="col-6 form-group">
									<label>End Channel:</label>
								</div>
								<div class="col-6 form-group">
								    <input type='button' class='buttons' value='-3' id='decEndButton' onClick='adjustEndChannel(-1);'>
									<input type='button' class='buttons' value='+3' id='incEndButton' onClick='adjustEndChannel(1);'>
								</div>
							</div>
							<div class='row'>
								<div class="col-6 form-group">
									<label>Both Channels:</label>
								</div>
								<div class="col-6 form-group">
								    <input type='button' class='buttons' value='-3' id='decBothButton' onClick='adjustBothChannels(-1);'>
									<input type='button' class='buttons' value='+3' id='incBothButton' onClick='adjustBothChannels(1);'>
								</div>
							</div>

							<div class='row stringRow' style='display: none;'>
								<div class="col-6 form-group">
									<label for="testModeStartString">Start String:</label>
									<input class="form-control" type='number' min='1' max='1' value='1' id='startString' onChange='StringsChanged();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'>
								</div>
								<div class="col-6 form-group">
									<label for="testModeEndString">End String:</label>
									<input class="form-control" type='number' min='1' max='1' value='1' id='endString' onChange='StringsChanged();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'>
								</div>
							</div>
							<div class='row stringRow' style='display: none;'>
								<div class="col-6 form-group">
								    <input type='button' class='buttons' value='-1' onClick='AdjustStartString(-1);'>
									<input type='button' class='buttons' value='+1' onClick='AdjustStartString(1);'>
								</div>
								<div class="col-6 form-group">
								    <input type='button' class='buttons' value='-1' onClick='AdjustEndString(-1);'>
									<input type='button' class='buttons' value='+1' onClick='AdjustEndString(1);'>
								</div>
							</div>

							<div class="mt-2 mb-1">
								<b >Update Interval: </b> 
								<input id="testModeCycleMS" type="range"  min="100" max="5000" value="1000" step="100"/> 
								<small  class="form-text text-muted">
									<span id='testModeCycleMSText'>1000</span><span> ms</span>
								</small>
								
							</div>
							<div>

								Color Order:
								<select id='colorOrder' onChange='SetTestMode();'>
									<option>RGB</option>
									<option>RBG</option>
									<option>GRB</option>
									<option>GBR</option>
									<option>BRG</option>
									<option>BGR</option>
								</select>
							</div>
						</div>

					</div>
					<div class="col-md-9">

						<h2>RGB Test Patterns</h2>
						<div class="callout callout-primary">
							<p><b>Note:</b> RGB patterns have NO knowledge of output setups, models, etc...  "R" is the first channel, "G" is the second, etc... If channels do not line up, the colors displayed on pixels may not match.</p>
						</div>
						<div class="row">
							<div class="col-md-6">
								<div class="backdrop">
									<h3>Chase Patterns</h3>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBChase-RGB' id='RGBChase-RGB' checked onChange='SetTestMode();'><label class="custom-control-label" for='RGBChase-RGB'>Chase: R-G-B</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBChase-RGBA' id='RGBChase-RGBA' onChange='SetTestMode();'><label class="custom-control-label" for='RGBChase-RGBA'>Chase: R-G-B-All</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBChase-RGBN' id='RGBChase-RGBN' onChange='SetTestMode();'><label class="custom-control-label" for='RGBChase-RGBN'>Chase: R-G-B-None</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBChase-RGBAN' id='RGBChase-RGBAN' onChange='SetTestMode();'><label class="custom-control-label" for='RGBChase-RGBAN'>Chase: R-G-B-All-None</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBChase-RGBCustom' id='RGBChase-RGBCustom' onChange='SetTestMode();'><label class="custom-control-label" for='RGBChase-RGBCustom'>Chase: Custom Pattern: </label></div>
									<div class="form-group">
				
										<input id='testModeRGBCustomPattern' size='36' maxlength='72' type="text" value='FF000000FF000000FF' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'> 
										<small  class="form-text text-muted">(6 hex digits per RGB triplet)</small>
									</div>	

								</div>
							</div>
							<div class="col-md-6">
								<div class="backdrop ">
									<h3>Cycle Patterns</h3>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBCycle-RGB' id='RGBCycle-RGB' onChange='SetTestMode();'><label class="custom-control-label" for='RGBCycle-RGB'>Cycle: R-G-B</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBCycle-RGBA' id='RGBCycle-RGBA' onChange='SetTestMode();'><label class="custom-control-label" for='RGBCycle-RGBA'>Cycle: R-G-B-All</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBCycle-RGBN' id='RGBCycle-RGBN' onChange='SetTestMode();'><label class="custom-control-label" for='RGBCycle-RGBN' >Cycle: R-G-B-None</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBCycle-RGBAN' id='RGBCycle-RGBAN' onChange='SetTestMode();'><label class="custom-control-label" for='RGBCycle-RGBAN'>Cycle: R-G-B-All-None</label></div>
									<div class="testPatternOptionRow custom-control custom-radio"><input type='radio' class="custom-control-input" name='testModeMode' value='RGBCycle-RGBCustom' id='RGBCycle-RGBCustom' onChange='SetTestMode();'><label class="custom-control-label" for='RGBCycle-RGBCustom'>Cycle: Custom Pattern: </label> </div>
									
									<div class="form-group">
								
										<input id='testModeRGBCycleCustomPattern' size='36' maxlength='72' type="text" value='FF000000FF000000FF' onChange='SetTestMode();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'> 
										<small  class="form-text text-muted">(6 hex digits per RGB triplet)	</small>
									</div>			
									
								</div>
							</div>
						</div>

                        <hr class="mt-4 mb-4">
						<h2>Solid Color Test Pattern</h2>
						<div class="backdrop mt-3">
						<div class="row">
							<div class="col-auto displayTestingFillOptionBoxHeader">
								<div class="testPatternOptionRow custom-control custom-radio">
									<input  class="custom-control-input" type='radio' name='testModeMode' value='RGBFill' id="RGBFill" onChange='SetTestMode();'>
									<label for="RGBFill" class="custom-control-label"><h3>Fill Color:</h3></label>
								</div>
								<div class="color-box"></div>
							</div>
							<div class="col-auto ml-auto">
								<input type=button class="buttons" onClick='AppendFillToCustom();' value='Append Color To Custom Pattern'>

							</div>
						</div>
							<div class="row">
								
								<div class="col-sm-4 testModeColorRange"><span>R: </span><input id="testModeColorR" type="range"  min="0" max="255" value="255" step="1"/>  <span id='testModeColorRText'>255</span><span></span></div>
								<div class="col-sm-4 testModeColorRange"><span>G: </span><input id="testModeColorG" type="range"  min="0" max="255" value="0" step="1"/> </span> <span id='testModeColorGText'>0</span><span></span></div>
								<div class="col-sm-4 testModeColorRange"><span>B: </span><input id="testModeColorB" type="range"  min="0" max="255" value="255" step="1"/>  <span id='testModeColorBText'>255</span><span></span></div>
	
							</div>
						</div>
						<hr class="mt-4 mb-4">
						<h2>Single Channel Patterns:</h2>
							<div class="backdrop">
														
				
							<span ><b>&nbsp;Channel Data Value: </b></span>
	
							<div><input id="testModeColorS" type="range"  min="0" max="255" value="255" step="1"/> </div>
							
							<div><span  id='testModeColorSText'>255</span></div>


									<div class="row">
										<div class="col-auto">
											<div class="form-row">
												<div class="testChannelOptionRow custom-control custom-radio">
													<input  class="custom-control-input" type='radio' name='testModeMode' value='SingleChase' id='SingleChase' onChange='SetTestMode();'>
													<label for="SingleChase" class="custom-control-label"><b>Chase</b></label>
												</div>
												<div class="form-col ml-2 pt-1">
						
						Chase Size:
						<select id='testModeChaseSize' onChange='SetTestMode();'>
							<option value='2'>2</option>
							<option value='3'>3</option>
							<option value='4'>4</option>
							<option value='5'>5</option>
							<option value='6'>6</option>
						</select>
</div>
											</div>
										

										</div>
										<div class="col-auto">
											<div class="testChannelOptionRow custom-control custom-radio">
												<input  class="custom-control-input" type='radio' name='testModeMode' value='SingleFill' id='SingleFill' onChange='SetTestMode();'>
												<label for="SingleFill" class="custom-control-label"><b>Fill</b></label>
											</div>
										</div>
									</div>	

						
					
							</div>
					</div>
				</div>
							


				
		</div>
					<div id='tab-sequence' class="tab-pane fade" role="tabpanel" aria-labelledby="interface-settings-tab">
						<div>
								<div>
									<table border='0' cellspacing='3'>
										<tr><td>Sequence:</td>
												<td><?php PrintSequenceOptions();?></td></tr>
										<tr><td>Start Time:</td>
												<td><input type='text' size='4' maxlength='4' value='0' id='startSecond'> (Seconds from beginning of sequence)</td></tr>

										<tr><td><input type='button' class="buttons" value='Play' onClick='PlaySequence();' id='playSequence'><input type='button' class="buttons" value='Stop' onClick='StopSequence();' id='stopSequence'></td>
												<td>Play/stop the selected sequence</td></tr>
										<tr><td><input type='button' class="buttons" value='Pause/UnPause' onClick='ToggleSequencePause();'></td>
												<td>Pause a running sequence or UnPause a paused sequence</td></tr>
										<tr><td><input type='button' class="buttons" value='Step' onClick='SingleStepSequence();'></td>
												<td>Single-step a paused sequence one frame</td></tr>
										<tr><td><input type='button' class="buttons" value='Step Back' onClick='SingleStepSequenceBack();'></td>
												<td>Single-step a paused sequence backwards one frame</td></tr>
									</table>
									<br>
									<div class="callout">
									<h4>Sequence Testing Limitations:</h4>
									<ol>
										<li>This page is for testing sequences, it does not test audio or video or synchronization of a sequence with any media file.  It does test Master/Remote sequence synchronization.</li>
										<li>The Sequence Testing functionality currently only works when FPP is in an idle state and no playlists are playing.  If a playlist starts while testing a sequence, the sequence being tested will be stopped automatically.</li>
									</ol>
									</div>
									

								</div>
						</div>
					</div>
				</div>
				</div>
			</div>	  	  
	    </div>
  </div>


  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
