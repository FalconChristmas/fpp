<!DOCTYPE html>
<html lang="en">
<?php
require_once 'config.php';
require_once 'common.php';
if (file_exists(__DIR__ . "/fppdefines.php")) {
	include_once __DIR__ . '/fppdefines.php';
} else {
	include_once __DIR__ . '/fppdefines_unknown.php';
}

function PrintSequenceOptions()
{
	global $sequenceDirectory;
	$first = 1;
	echo "<select id=\"selSequence\" size=\"1\">";
	foreach (scandir($sequenceDirectory) as $seqFile) {
		if ($seqFile != '.' && $seqFile != '..' && !preg_match('/.eseq$/', $seqFile)) {
			echo "<option value=\"" . $seqFile . "\"";
			if ($first) {
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
$rgbStr = "RGB";
$rgbColorList = "R-G-B";

if (isset($settings['useRGBLabels']) && ($settings['useRGBLabels'] == '0')) {
	$rgbLabels[0] = 'A';
	$rgbLabels[1] = 'B';
	$rgbLabels[2] = 'C';
	$rgbColors[0] = 'A';
	$rgbColors[1] = 'B';
	$rgbColors[2] = 'C';
	$rgbStr = "ABC";
	$rgbColorList = "A-B-C";
} else {
	$rgbLabels[0] = 'R';
	$rgbLabels[1] = 'G';
	$rgbLabels[2] = 'B';
	$rgbColors[0] = 'Red';
	$rgbColors[1] = 'Green';
	$rgbColors[2] = 'Blue';
	$rgbStr = "RGB";
	$rgbColorList = "R-G-B";
	$settings['useRGBLabels'] = 1;
}

$testStartChannel = 1;
$testEndChannel = FPPD_MAX_CHANNELS;
if (file_exists($mediaDirectory . "/fpp-info.json")) {
	$content = file_get_contents($mediaDirectory . "/fpp-info.json");
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
	<?php include 'common/htmlMeta.inc';
	include 'common/menuHead.inc'; ?>
	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<link rel="stylesheet" type="text/css" href="jquery/colpick/css/colpick.css">
	<link rel="stylesheet" type="text/css" href="css/jquery.colpick.css">
	<script type="text/javascript" src="jquery/colpick/js/colpick.js"></script>
	<title><? echo $pageTitle; ?></title>

</head>

<body onunload='HandlePageUnload();'>

	<script type="text/javascript">
		if (!window.console) console = { log: function () { } };

		var modelInfos = [];
		var lastEnabledState = 0;

		// fppd runs one test at a time, but the Channel Testing and Channel Fader
		// tabs each have their own Enable Test Mode checkbox. Remember which tab
		// started the running test so switching tabs shows the truth rather than an
		// unticked box sitting next to output that is still running. sessionStorage
		// carries the owner across navigating away from and back to this page.
		var TEST_OWNER_KEY = 'fppTestOwner';
		var testOwner = null;
		var pageUnloading = false;

		function StoreTestOwner(owner) {
			// The Test Stop we fire on unload usually doesn't make it out before the
			// page goes away, so keep the stored owner rather than clearing it - the
			// test is most likely still running when we come back.
			if (pageUnloading && !owner) {
				return;
			}
			testOwner = owner;
			try {
				if (owner) {
					sessionStorage.setItem(TEST_OWNER_KEY, owner);
				} else {
					sessionStorage.removeItem(TEST_OWNER_KEY);
				}
			} catch (e) {
				// sessionStorage blocked; the in-memory owner still covers this page view
			}
		}

		function LoadTestOwner() {
			try {
				return sessionStorage.getItem(TEST_OWNER_KEY);
			} catch (e) {
				return null;
			}
		}

		// Each tab warns when the other tab owns the running test, since enabling
		// test mode here replaces it.
		function UpdateTakeoverNotices() {
			$('#channelTestTakeover').toggleClass('d-none', testOwner != 'dmx');
			$('#dmxTestTakeover').toggleClass('d-none', testOwner != 'channels');
		}

		// Point both tabs' checkboxes at the one test fppd is actually running.
		// 'enabled' is fppd's real state, from api/testmode.
		function ApplyTestOwnership(enabled) {
			var owner = null;
			if (enabled) {
				// A test we never started (another browser, or a page load with no
				// stored owner) is attributed to the Channel Testing tab, whose
				// controls GetTestMode() fills in from the running test.
				owner = testOwner || LoadTestOwner() || 'channels';
			}
			StoreTestOwner(owner);

			lastEnabledState = (owner == 'channels') ? 1 : 0;
			dmxLastEnabled = (owner == 'dmx') ? 1 : 0;
			$('#testModeEnabled').prop('checked', owner == 'channels');
			$('#dmxTestEnabled').prop('checked', owner == 'dmx');
			if (owner != 'dmx') {
				DMXSineStop();
			}
			UpdateTakeoverNotices();
		}

		// Re-read fppd's test state when switching between the top-level tabs.
		function RefreshTestState() {
			$.ajax({
				url: "api/testmode",
				dataType: 'json',
				success: function (data) {
					ApplyTestOwnership(data.enabled ? true : false);
				}
			});
		}

		function StopRunningTest() {
			var data = {
				"command": "Test Stop",
				"multisyncCommand": $('#multisyncEnabled').is(':checked') || $('#dmxMultisyncEnabled').is(':checked'),
				"multisyncHosts": "",
				"args": []
			};
			$.post("api/command", JSON.stringify(data)).done(function () {
				ApplyTestOwnership(false);
			}).fail(function () {
				DialogError("Failed to stop Test Mode", "Stop failed");
			});
		}

		function StringsChanged() {
			var id = parseInt($('#modelName').val());

			var startChan = modelInfos[id].StartChannel + (modelInfos[id].ChannelsPerString * (parseInt($('#startString').val()) - 1));
			var endChan = modelInfos[id].StartChannel + (modelInfos[id].ChannelsPerString * (parseInt($('#endString').val())) - 1);

			$('#testModeStartChannel').val(startChan);
			$('#testModeEndChannel').val(endChan);

			SetTestMode();
		}

		function AdjustStartString(delta = 1) {
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

		function AdjustEndString(delta = 1) {
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

		// How many channels one pixel occupies.  Test patterns are written into
		// fppd's channel data, which is always R,G,B[,W]; the hardware colour order
		// is applied afterwards by the output, so all this page needs is the stride.
		function ChannelsPerPixel() {
			var cpp = parseInt($('#channelsPerPixel').val());
			// 1, 3 and 4 are the only node widths FPP produces, and RGBFill clamps
			// anything else to 3, so offering more would promise a stride the
			// backend will not use.
			return (cpp == 1 || cpp == 4) ? cpp : 3;
		}

		function PixelIsRGBW() {
			return ChannelsPerPixel() == 4;
		}

		// The White channel slider only applies to 4-channel (RGBW) pixels.
		// Disable and zero it otherwise so a stray W value can't change the
		// channel stride sent to the backend.
		function UpdateWhiteSliderState() {
			var isRGBW = PixelIsRGBW();
			$('#testModeColorW').prop('disabled', !isRGBW);
			if (!isRGBW) {
				$('#testModeColorW').val(0);
				$('#testModeColorWText').html(0);
			}
		}

		function UpdateIncrementFromChannelsPerPixel() {
			$('#channelIncrement').val(ChannelsPerPixel());
			UpdateWhiteSliderState();
			UpdateStrideNotice();
			SetButtonIncrements();
		}

		// Channel range the stride notice is describing, or [null, null] for a
		// single model.  Held so the notice can be refreshed when only the width
		// selection changes.
		var noticeRange = [null, null];

		// Call out the two cases where the stride will not line up with the hardware:
		// a range spanning models of different node widths, and single colour strings,
		// which the RGB patterns cannot address one channel at a time.
		function UpdateStrideNotice() {
			var msgs = [];
			var start = noticeRange[0];
			var end = noticeRange[1];

			if (start != null) {
				var widths = {};
				for (var i = 0; i < modelInfos.length; i++) {
					var m = modelInfos[i];
					if (m.StartChannel > 0 && m.StartChannel <= end && m.EndChannel >= start) {
						widths[m.ChannelCountPerNode] = true;
					}
				}
				var list = Object.keys(widths).sort();
				if (list.length > 1) {
					msgs.push('Models in this range use ' + list.join(' and ') + ' channels per'
						+ ' pixel. A single stride cannot align them all &mdash; select an'
						+ ' individual model to test it accurately.');
				}
			}

			if (ChannelsPerPixel() == 1) {
				msgs.push('The RGB test patterns always write 3 channels per pixel. On single'
					+ ' colour strings use the Single Channel patterns, which step one'
					+ ' channel at a time.');
			}

			if (msgs.length > 0) {
				$('#strideNotice').html(msgs.join(' ')).show();
			} else {
				$('#strideNotice').hide();
			}
		}

		function UpdateStartEndFromModel() {
			var val = $('#modelName').val();
			if (val.indexOf(',') != -1) {
				var parts = val.split(',');
				$('#testModeStartChannel').val(parseInt(parts[0]));
				$('#testModeEndChannel').val(parseInt(parts[1]));
				$('.stringRow').hide();
				// A whole-house range has no single node width to read; fall back to
				// 3 but leave the control editable so an all-RGBW or all-single-colour
				// setup can still pick its own stride.
				$('#channelsPerPixel').val(3);
				$('#channelIncrement').val(3);
				noticeRange = [parseInt(parts[0]), parseInt(parts[1])];
				UpdateWhiteSliderState();
				UpdateStrideNotice();
				SetButtonIncrements();
			} else {
				var id = parseInt(val);
				$('#testModeStartChannel').val(modelInfos[id].StartChannel);
				$('#testModeEndChannel').val(modelInfos[id].EndChannel);

				// The select only offers the widths FPP produces; a hand-entered
				// oddity falls back to 3, which is what RGBFill would clamp it to
				// anyway, so the control and ChannelsPerPixel() cannot disagree.
				$('#channelsPerPixel').val(modelInfos[id].ChannelCountPerNode);
				if ($('#channelsPerPixel').val() == null) {
					$('#channelsPerPixel').val(3);
				}
				noticeRange = [null, null];
				UpdateWhiteSliderState();
				UpdateStrideNotice();

				if (modelInfos[id].StringCount > 1) {
					$('#startString').attr('max', modelInfos[id].StringCount);
					$('#startString').val(1);
					$('#endString').attr('max', modelInfos[id].StringCount);
					$('#endString').val(modelInfos[id].StringCount);
					$('#channelIncrement').val(modelInfos[id].ChannelsPerString);
					$('.stringRow').show();
				} else {
					// Step by the model's real node width even when the selector had
					// to clamp it - the increment only drives the +/- buttons.
					$('#channelIncrement').val(modelInfos[id].ChannelCountPerNode);
					$('.stringRow').hide();
				}
				SetButtonIncrements();
			}

			if (lastEnabledState) {
				var data = {
					"command": "Test Stop",
					"multisyncCommand": $('#multisyncEnabled').is(':checked'),
					"multisyncHosts": "",
					"args": []
				};
				var postData = JSON.stringify(data);
				console.log(postData);
				$.post("api/command", postData).done(function (data) {
					SetTestMode();
					//			$.jGrowl("Test Mode Disabled");
				}).fail(function (data) {
					DialogError("Failed to set Test Mode", "Setup failed");
				});
			} else {
				SetTestMode();
			}
		}

		function GetTestMode() {
			$.ajax({
				url: "api/testmode",
				async: false,
				dataType: 'json',
				success: function (data) {
					if (data.enabled) {
						ApplyTestOwnership(true);

						if (data.hasOwnProperty('cycleMS')) {
							$("#testModeCycleMSText").html(data.cycleMS);
							$("#testModeCycleMS").val(data.cycleMS);
						} else {
							$("#testModeCycleMSText").html(1000);
							$("#testModeCycleMS").val(1000);
						}
						if (data.mode == "SingleChase") {
							$("input[name=testModeMode][value=SingleChase]").prop('checked', true);
							$('#testModeChaseSize').val(data.chaseSize);
							$('#testModeColorSText').html(data.chaseValue);
							$("#testModeColorS").val(data.chaseValue);
						}
						else if (data.mode == "RGBChase") {
							$("input[name=testModeMode][value=" + data.subMode + "]").prop('checked', true);
							if (data.subMode == "RGBChase-RGBCustom")
								$('#testModeRGBCustomPattern').val(data.colorPattern);
						}
						else if (data.mode == "RGBCycle") {
							$("input[name=testModeMode][value=" + data.subMode + "]").prop('checked', true);
							if (data.subMode == "RGBCycle-RGBCustom")
								$('#testModeRGBCycleCustomPattern').val(data.colorPattern);
						}
						else if (data.mode == "RGBFill") {
							$("input[name=testModeMode][value=RGBFill]").prop('checked', true);
							$("#testModeColorRText").html(data.color1);
							$("#testModeColorGText").html(data.color2);
							$("#testModeColorBText").html(data.color3);
							$("#testModeColorR").val(data.color1);
							$("#testModeColorG").val(data.color2);
							$("#testModeColorB").val(data.color3);
							var rgb = {
								r: data.color1,
								g: data.color2,
								b: data.color3
							};
							$('.color-box').colpickSetColor(rgb).css('background-color', $.colpick.rgbToHex(rgb));
						}
					}
					else {
						ApplyTestOwnership(false);
					}
				},
				failure: function (data) {
					ApplyTestOwnership(false);
				}
			});
		}

		function SetTestMode() {
			var enabled = 0;
			var mode = "singleChase";
			var cycleMS = $('#testModeCycleMSText').html();
			var colorS = parseInt($('#testModeColorSText').html());
			var colorR = parseInt($('#testModeColorRText').html());
			var colorG = parseInt($('#testModeColorGText').html());
			var colorB = parseInt($('#testModeColorBText').html());
			var colorW = parseInt($('#testModeColorWText').html());
			// Test patterns are written straight into fppd's channel data, which is
			// always R,G,B[,W]; the output driver applies the hardware colour order on
			// the way to the wire.  Permuting here as well would apply it twice.
			var color1 = colorR;
			var color2 = colorG;
			var color3 = colorB;
			var startChannel = parseInt($('#testModeStartChannel').val());
			var endChannel = parseInt($('#testModeEndChannel').val());
			var chaseSize = parseInt($('#testModeChaseSize').val());
			var maxChannel = 8 * 1024 * 1024;
			var channelSetType = "channelRange";

			if (startChannel < 1 || startChannel > maxChannel || isNaN(startChannel)) {
				startChannel = 1;
			}

			if (endChannel < 1 || endChannel > maxChannel || isNaN(endChannel)) {
				endChannel = maxChannel;
			}

			if (endChannel < startChannel) {
				endChannel = startChannel;
			}

			var selected = $("#tab-channels input[type='radio']:checked");
			if (selected.length > 0) {
				mode = selected.val();
			}

			if ($('#testModeEnabled').is(':checked')) {
				enabled = 1;
			}

			if (enabled || lastEnabledState) {
				var data = {
					"command": "Test Start",
					"multisyncCommand": $('#multisyncEnabled').is(':checked'),
					"multisyncHosts": "",
					"args": []
				};
				var channelSet = "" + startChannel + "-" + endChannel;
				data["args"].push(cycleMS);

				if (mode == "SingleChase") {
					data["args"].push("Single Channel Chase");
					data["args"].push(channelSet);
					data["args"].push(colorS.toString());
					data["args"].push(chaseSize.toString());
				} else if (mode.substring(0, 9) == "RGBChase-") {
					if (mode == "RGBChase-RGBCustom") {
						data["args"].push("Custom Chase");
						data["args"].push(channelSet);
						data["args"].push($('#testModeRGBCustomPattern').val());
						if (PixelIsRGBW()) {
							data["args"].push("4");
						}
					} else {
						data["args"].push("RGB Chase");
						data["args"].push(channelSet);
						if (mode == "RGBChase-RGB") {
							data["args"].push("R-G-B");
						} else if (mode == "RGBChase-RGBW") {
							data["args"].push("R-G-B-W");
						} else if (mode == "RGBChase-RGBA") {
							data["args"].push("R-G-B-All");
						} else if (mode == "RGBChase-RGBWA") {
							data["args"].push("R-G-B-W-All");
						} else if (mode == "RGBChase-RGBN") {
							data["args"].push("R-G-B-None");
						} else if (mode == "RGBChase-RGBWN") {
							data["args"].push("R-G-B-W-None");
						} else if (mode == "RGBChase-RGBAN") {
							data["args"].push("R-G-B-All-None");
						} else if (mode == "RGBChase-RGBWAN") {
							data["args"].push("R-G-B-W-All-None");
						}
					}
				} else if (mode.substring(0, 9) == "RGBCycle-") {
					if (mode == "RGBCycle-RGBCustom") {
						data["args"].push("Custom Cycle");
						data["args"].push(channelSet);
						data["args"].push($('#testModeRGBCycleCustomPattern').val());
						if (PixelIsRGBW()) {
							data["args"].push("4");
						}
					} else {
						data["args"].push("RGB Cycle");
						data["args"].push(channelSet);
						if (mode == "RGBCycle-RGB") {
							data["args"].push("R-G-B");
						} else if (mode == "RGBCycle-RGBW") {
							data["args"].push("R-G-B-W");
						} else if (mode == "RGBCycle-RGBA") {
							data["args"].push("R-G-B-All");
						} else if (mode == "RGBCycle-RGBWA") {
							data["args"].push("R-G-B-W-All");
						} else if (mode == "RGBCycle-RGBN") {
							data["args"].push("R-G-B-None");
						} else if (mode == "RGBCycle-RGBWN") {
							data["args"].push("R-G-B-W-None");
						} else if (mode == "RGBCycle-RGBAN") {
							data["args"].push("R-G-B-All-None");
						} else if (mode == "RGBCycle-RGBWAN") {
							data["args"].push("R-G-B-W-All-None");
						}
					}
				} else if (mode == "SingleFill") {
					data["args"].push("Single Channel Fill");
					data["args"].push(channelSet);
					data["args"].push(colorS.toString());
				} else if (mode == "RGBFill") {
					data["args"].push("RGB Single Color");
					data["args"].push(channelSet);
					var c = (color1 << 16) + (color2 << 8) + color3;
					var hexColor = c.toString(16).padStart(6, '0');
					// For RGBW color orders, always send the 4th (White) channel - even
					// when W is 0 - so the backend fills with a 4-channel stride and the
					// pixels stay aligned for pure colors.
					if (PixelIsRGBW()) {
						hexColor += colorW.toString(16).padStart(2, '0');
					}
					data["args"].push("#" + hexColor); //color
				}

				if (!enabled) {
					data = {
						"command": "Test Stop",
						"multisyncCommand": $('#multisyncEnabled').is(':checked'),
						"multisyncHosts": "",
						"args": []
					};
				}
				//data.enabled = enabled;
				//data.channelSet = channelSet;
				//data.channelSetType = channelSetType;

				var postData = JSON.stringify(data);
				console.log(postData);
				$.post("api/command", postData).done(function (data) {
					//$.jGrowl("Test Mode Set");
					//console.log(data);
				}).fail(function (data) {
					DialogError("Failed to set Test Mode", "Setup failed");
				});
			}

			lastEnabledState = enabled;

			if (enabled) {
				// This tab now owns the one fppd test, replacing anything the Channel
				// Fader tab had running.
				StoreTestOwner('channels');
				$('#dmxTestEnabled').prop('checked', false);
				dmxLastEnabled = 0;
				DMXSineStop();
			} else if (testOwner == 'channels') {
				StoreTestOwner(null);
			}
			UpdateTakeoverNotices();
		}

		function HandlePageUnload() {
			pageUnloading = true;
			DisableTestMode();
			DisableDMXTestMode();
		}

		function DisableTestMode() {
			$('#testModeEnabled').prop('checked', false);
			SetTestMode();
		}

		function SetButtonIncrements() {
			var delta = $('#channelIncrement').val();

			$('#incStartButton').val('+' + delta);
			$('#decStartButton').val('-' + delta);
			$('#incBothButton').val('+' + delta);
			$('#decBothButton').val('-' + delta);
			$('#incEndButton').val('+' + delta);
			$('#decEndButton').val('-' + delta);
		}

		function adjustBothChannels(mult = 1) {
			if (mult > 0) {
				adjustEndChannel(mult, false);
				adjustStartChannel(mult);
			} else {
				adjustStartChannel(mult, false);
				adjustEndChannel(mult);
			}
		}

		function adjustStartChannel(mult = 1, startTest = true) {
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

			if (startTest) {
				SetTestMode();
			}
		}

		function adjustEndChannel(mult = 1, startTest = true) {
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

			if (startTest) {
				SetTestMode();
			}
		}

		function dec2hex(i) {
			return (i + 0x100).toString(16).substr(-2).toUpperCase();
		}

		function AppendFillToCustom() {
			var colorR = dec2hex(parseInt($('#testModeColorRText').html()));
			var colorG = dec2hex(parseInt($('#testModeColorGText').html()));
			var colorB = dec2hex(parseInt($('#testModeColorBText').html()));

			var newTriplet = colorR + colorG + colorB;
			if (PixelIsRGBW()) {
				newTriplet += dec2hex(parseInt($('#testModeColorWText').html()) || 0);
			}

			var currentValue = $('#testModeRGBCustomPattern').val();
			$('#testModeRGBCustomPattern').val(currentValue + newTriplet);

			currentValue = $('#testModeRGBCycleCustomPattern').val();
			$('#testModeRGBCycleCustomPattern').val(currentValue + newTriplet);

			SetTestMode();
		}

		function AppendColorPickerToCustom() {
			var hexColor = $('#customPatternColorPicker').val();
			// Remove the # from hex color
			var colorTriplet = hexColor.substring(1).toUpperCase();
			if (PixelIsRGBW()) {
				colorTriplet += '00'; // color picker has no W; keep pixels 4-channel aligned
			}

			var currentValue = $('#testModeRGBCycleCustomPattern').val();
			$('#testModeRGBCycleCustomPattern').val(currentValue + colorTriplet);

			SetTestMode();
		}

		function ClearCustomPattern() {
			$('#testModeRGBCycleCustomPattern').val('');
			SetTestMode();
		}

		function AppendColorPickerToCustomChase() {
			var hexColor = $('#customChasePatternColorPicker').val();
			// Remove the # from hex color
			var colorTriplet = hexColor.substring(1).toUpperCase();
			if (PixelIsRGBW()) {
				colorTriplet += '00'; // color picker has no W; keep pixels 4-channel aligned
			}

			var currentValue = $('#testModeRGBCustomPattern').val();
			$('#testModeRGBCustomPattern').val(currentValue + colorTriplet);

			SetTestMode();
		}

		function ClearCustomChasePattern() {
			$('#testModeRGBCustomPattern').val('');
			SetTestMode();
		}

		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		// Sequence Testing Functions
		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		function PlaySequence() {
			var sequence = $('#selSequence').val();
			var startSecond = $('#startSecond').val();

			$.get("api/sequence/" + sequence + "/start/" + startSecond
			).done(function () {
				$.jGrowl("Started sequence " + sequence, { themeState: 'success' });
			}).fail(function () {
				DialogError("Failed to start sequence", "Start failed");
			});
		}

		function StopSequence() {
			var url = "api/sequence/current/stop";
			var sequence = $('#selSequence').val();

			$.get(url
			).done(function () {
				$.jGrowl("Stop sequence " + sequence, { themeState: 'danger' });
			}).fail(function () {
				DialogError("Failed to stop sequence", "Stop failed");
			});

		}
		function UpdateTestModeFillColors() {
			var rgb = {
				r: parseInt($('#testModeColorR').val()),
				g: parseInt($('#testModeColorG').val()),
				b: parseInt($('#testModeColorB').val())
			}
			$('#testModeColorRText').html(rgb.r);
			$('#testModeColorGText').html(rgb.g);
			$('#testModeColorBText').html(rgb.b);
			$('.color-box').colpickSetColor($.colpick.rgbToHex(rgb));
		}
		function UpdateTestModeFillColorW() {
			var w = parseInt($('#testModeColorW').val());
			$('#testModeColorWText').html(w);
		}

		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		// Channel Fader Functions
		/////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////
		var dmxValues = [];
		var dmxLastEnabled = 0;
		const DMX_CHANNELS_PER_TAB = 16;
		const DMX_STATIC_CYCLE_MS = 86400000; // ~24h, prevents visible cycling

		function dmxToHex(v) {
			v = parseInt(v);
			if (isNaN(v) || v < 0) v = 0;
			if (v > 255) v = 255;
			return ('00' + v.toString(16).toUpperCase()).slice(-2);
		}

		// Returns a stable, light pastel background colour for a fixture index
		// so adjacent fixtures are visually distinct on the DMX test grid.
		var dmxDarkMode = document.documentElement.getAttribute('data-bs-theme') === 'dark';
		function dmxFixtureColor(idx) {
			var hue = (idx * 67) % 360;
			// Dark mode: use a mid-dark background so dark text stays readable.
			return dmxDarkMode ? 'hsl(' + hue + ', 45%, 28%)' : 'hsl(' + hue + ', 70%, 82%)';
		}
		function dmxFixtureBorderColor(idx) {
			var hue = (idx * 67) % 360;
			return dmxDarkMode ? 'hsl(' + hue + ', 50%, 50%)' : 'hsl(' + hue + ', 55%, 45%)';
		}
		function dmxFixtureTextColor(idx) {
			// Light mode backgrounds (~82% lightness) need dark text.
			// Dark mode backgrounds (~28% lightness) need light text.
			return dmxDarkMode ? '#e8e8e8' : '#222';
		}

		// Find the model that contains the given absolute channel. Models with
		// huge channel counts (matrices, props) are skipped so the Channel Fader
		// only highlights small fixtures from model-overlays.json.
		function dmxFindFixtureForChannel(absCh) {
			if (!modelInfos || !modelInfos.length) return null;
			for (var i = 0; i < modelInfos.length; i++) {
				var m = modelInfos[i];
				if (!m || !m.StartChannel || !m.ChannelCount) continue;
				if (m.ChannelCount > 512) continue;
				var end = m.StartChannel + m.ChannelCount - 1;
				if (absCh >= m.StartChannel && absCh <= end) {
					return { model: m, index: i, relCh: absCh - m.StartChannel + 1 };
				}
			}
			return null;
		}

		// Simple HTML-escape for fixture names injected into markup.
		function dmxEscapeHtml(s) {
			return String(s)
				.replace(/&/g, '&amp;')
				.replace(/</g, '&lt;')
				.replace(/>/g, '&gt;')
				.replace(/"/g, '&quot;')
				.replace(/'/g, '&#39;');
		}

		// Apply the selected model's start channel and channel count to the
		// Channel Fader inputs and rebuild the slider grid. The dropdown stores
		// the index into modelInfos as its value; an empty value means
		// "manual" and leaves the current inputs alone.
		function DMXUpdateFromModel() {
			var val = $('#dmxModelName').val();
			if (val === '' || val === null) {
				return;
			}
			var id = parseInt(val);
			if (isNaN(id) || !modelInfos[id]) {
				return;
			}
			var m = modelInfos[id];
			var count = parseInt(m.ChannelCount);
			if (isNaN(count) || count < 1) count = 1;
			if (count > 512) count = 512; // DMX universe cap matches the input's max
			$('#dmxStartChannel').val(m.StartChannel);
			$('#dmxChannelCount').val(count);
			RebuildDMXSliders();
		}

		function RebuildDMXSliders() {
			DMXSineStop();
			var startCh = parseInt($('#dmxStartChannel').val());
			var count = parseInt($('#dmxChannelCount').val());
			var maxCh = <? echo FPPD_MAX_CHANNELS; ?>;

			if (isNaN(startCh) || startCh < 1) startCh = 1;
			if (startCh > maxCh) startCh = maxCh;
			if (isNaN(count) || count < 1) count = 1;
			if (count > 512) count = 512;
			if (startCh + count - 1 > maxCh) count = maxCh - startCh + 1;

			$('#dmxStartChannel').val(startCh);
			$('#dmxChannelCount').val(count);

			// Preserve existing slider values when rebuilding
			var prev = dmxValues.slice();
			dmxValues = [];
			for (var i = 0; i < count; i++) {
				dmxValues.push(i < prev.length ? prev[i] : 0);
			}

			var $nav = $('#dmxSliderTabsNav');
			var $content = $('#dmxSliderTabsContent');
			$nav.empty();
			$content.empty();

			var numTabs = Math.ceil(count / DMX_CHANNELS_PER_TAB);
			for (var t = 0; t < numTabs; t++) {
				var first = t * DMX_CHANNELS_PER_TAB;
				var last = Math.min(first + DMX_CHANNELS_PER_TAB, count) - 1;
				var firstCh = startCh + first;
				var lastCh = startCh + last;
				var tabId = 'dmxSliderTab-' + t;
				var navId = 'dmxSliderTabNav-' + t;
				var activeNav = (t === 0) ? ' active' : '';
				var activePane = (t === 0) ? ' show active' : '';

				$nav.append(
					'<li class="nav-item">' +
					'<a class="nav-link' + activeNav + '" id="' + navId + '" data-bs-toggle="tab" ' +
					'data-bs-target="#' + tabId + '" href="#' + tabId + '" role="tab" ' +
					'aria-controls="' + tabId + '" aria-selected="' + (t === 0 ? 'true' : 'false') + '">' +
					'Ch ' + firstCh + '-' + lastCh +
					'</a></li>'
				);

				var paneHtml = '<div class="tab-pane fade' + activePane + '" id="' + tabId +
					'" role="tabpanel" aria-labelledby="' + navId + '">';

				paneHtml += '<div class="dmxSliderGrid">';
				for (var i = first; i <= last; i++) {
					var ch = startCh + i;
					var fx = dmxFindFixtureForChannel(ch);
					var prevFx = (i > first) ? dmxFindFixtureForChannel(ch - 1) : null;
					var prevSameFixture = !!(fx && prevFx && fx.index === prevFx.index);
					var boxStyle = '';
					var fixtureLabel = '';
					var relLabel = '';
					var boxClass = 'dmxChannelBox';
					if (fx) {
						boxStyle =
							'background-color: ' + dmxFixtureColor(fx.index) + ';' +
							'border-color: ' + dmxFixtureBorderColor(fx.index) + ';' +
							'color: ' + dmxFixtureTextColor(fx.index) + ';';
						boxClass += ' dmxChannelBoxFixture';
						if (prevSameFixture) {
							boxClass += ' dmxChannelBoxFixtureCont';
						}
						// Only show the fixture name on the first channel of the
						// fixture in this tab to avoid repeating the label on
						// every box. Compute how many consecutive channels in
						// this tab belong to the same fixture so we can set a
						// min-width that lets the name span the full group width
						// without being clipped to the first 90px column.
						if (!prevSameFixture) {
							var groupSpan = 1;
							for (var j = i + 1; j <= last; j++) {
								var jFx = dmxFindFixtureForChannel(startCh + j);
								if (jFx && jFx.index === fx.index) groupSpan++;
								else break;
							}
							// Each box is 90px wide; gap between boxes is 8px.
							var nameMinWidth = groupSpan * 90 + (groupSpan - 1) * 8;
							fixtureLabel = '<div class="dmxFixtureName" title="' +
								dmxEscapeHtml(fx.model.Name) +
								'" style="min-width:' + nameMinWidth + 'px">' +
								dmxEscapeHtml(fx.model.Name) + '</div>';
						} else {
							fixtureLabel = '<div class="dmxFixtureName dmxFixtureNameSpacer">&nbsp;</div>';
						}
						relLabel = '<div class="dmxRelChannel">Ch ' + fx.relCh +
							' / ' + fx.model.ChannelCount + '</div>';
					}
					paneHtml +=
						'<div class="' + boxClass + '"' +
						(boxStyle ? ' style="' + boxStyle + '"' : '') + '>' +
						fixtureLabel +
						'<div class="dmxChannelLabel"><b>Ch ' + ch + '</b></div>' +
						relLabel +
						'<div class="dmxSliderWrapper">' +
						'<input type="range" min="0" max="255" step="1" ' +
						'value="' + dmxValues[i] + '" ' +
						'class="dmxSlider" data-idx="' + i + '">' +
						'</div>' +
						'<div><input type="number" min="0" max="255" step="1" ' +
						'value="' + dmxValues[i] + '" ' +
						'class="form-control form-control-sm dmxValueInput" data-idx="' + i + '"></div>' +
						'</div>';
				}
				paneHtml += '</div></div>';
				$content.append(paneHtml);
			}

			// Wire up events
			$('.dmxSlider').on('input', function () {
				var idx = parseInt($(this).attr('data-idx'));
				var v = parseInt($(this).val());
				dmxValues[idx] = v;
				$('.dmxValueInput[data-idx="' + idx + '"]').val(v);
			}).on('change', function () {
				var idx = parseInt($(this).attr('data-idx'));
				dmxValues[idx] = parseInt($(this).val());
				SetDMXTestMode();
			});
			$('.dmxValueInput').on('change', function () {
				var idx = parseInt($(this).attr('data-idx'));
				var v = parseInt($(this).val());
				if (isNaN(v) || v < 0) v = 0;
				if (v > 255) v = 255;
				$(this).val(v);
				dmxValues[idx] = v;
				$('.dmxSlider[data-idx="' + idx + '"]').val(v);
				SetDMXTestMode();
			});

			SetDMXTestMode();
		}

		function DMXSetAll(v) {
			DMXSineStop();
			for (var i = 0; i < dmxValues.length; i++) {
				dmxValues[i] = v;
			}
			$('.dmxSlider').val(v);
			$('.dmxValueInput').val(v);
			SetDMXTestMode();
		}

		var dmxSineTimer = null;
		var dmxSineStart = 0;
		const DMX_SINE_FRAME_MS = 50; // 20 fps

		function DMXSineTick() {
			var count = dmxValues.length;
			if (!count) return;

			var speed = parseFloat($('#dmxSineSpeed').val());
			if (isNaN(speed) || speed <= 0) speed = 0.5;
			var spread = parseFloat($('#dmxSineSpread').val());
			if (isNaN(spread) || spread < 0) spread = 1;

			var t = (performance.now() - dmxSineStart) / 1000.0;
			var phase = 2 * Math.PI * speed * t;
			// channelStep: how many radians of phase shift per channel
			var channelStep = (count > 1) ? (2 * Math.PI * spread / count) : 0;

			for (var i = 0; i < count; i++) {
				var s = Math.sin(phase + i * channelStep);
				var v = Math.round((s + 1) * 127.5); // 0..255
				if (v < 0) v = 0;
				if (v > 255) v = 255;
				dmxValues[i] = v;
				$('.dmxSlider[data-idx="' + i + '"]').val(v);
				$('.dmxValueInput[data-idx="' + i + '"]').val(v);
			}
			SetDMXTestMode();
		}

		function DMXSineStart() {
			if (dmxSineTimer) return;
			// Auto-enable test mode so the sine wave is actually output
			if (!$('#dmxTestEnabled').is(':checked')) {
				$('#dmxTestEnabled').prop('checked', true);
			}
			dmxSineStart = performance.now();
			dmxSineTimer = setInterval(DMXSineTick, DMX_SINE_FRAME_MS);
			$('#dmxSineStartBtn').prop('disabled', true);
			$('#dmxSineStopBtn').prop('disabled', false);
			DMXSineTick();
		}

		function DMXSineStop() {
			if (dmxSineTimer) {
				clearInterval(dmxSineTimer);
				dmxSineTimer = null;
			}
			$('#dmxSineStartBtn').prop('disabled', false);
			$('#dmxSineStopBtn').prop('disabled', true);
		}

		function SetDMXTestMode() {
			var enabled = $('#dmxTestEnabled').is(':checked') ? 1 : 0;
			var startCh = parseInt($('#dmxStartChannel').val());
			var count = dmxValues.length;

			if (!enabled && !dmxLastEnabled) {
				return;
			}

			var data;
			if (enabled && count > 0) {
				// Disable the channel-test page's test if it was running so we don't
				// fight over channel ownership
				if (lastEnabledState) {
					$('#testModeEnabled').prop('checked', false);
					lastEnabledState = 0;
				}

				var pattern = '';
				for (var i = 0; i < count; i++) {
					pattern += dmxToHex(dmxValues[i]);
				}
				// Neither the pattern nor the range is padded out to a whole RGB
				// triplet: fppd fills a trailing partial pixel, so the fixture's
				// channels are driven exactly as selected. Rounding the range up (as
				// this did while the test patterns skipped that partial pixel) drove
				// up to two channels past the end of the fixture.
				var endCh = startCh + count - 1;

				data = {
					"command": "Test Start",
					"multisyncCommand": $('#dmxMultisyncEnabled').is(':checked'),
					"multisyncHosts": "",
					"args": [
						String(DMX_STATIC_CYCLE_MS),
						"Custom Chase",
						startCh + "-" + endCh,
						pattern
					]
				};
			} else {
				data = {
					"command": "Test Stop",
					"multisyncCommand": $('#dmxMultisyncEnabled').is(':checked'),
					"multisyncHosts": "",
					"args": []
				};
			}

			$.post("api/command", JSON.stringify(data)).fail(function () {
				DialogError("Failed to set DMX Test Mode", "Setup failed");
			});

			dmxLastEnabled = enabled;

			if (enabled && count > 0) {
				StoreTestOwner('dmx');
			} else if (testOwner == 'dmx') {
				StoreTestOwner(null);
			}
			UpdateTakeoverNotices();
		}

		function DisableDMXTestMode() {
			DMXSineStop();
			$('#dmxTestEnabled').prop('checked', false);
			if (dmxLastEnabled) {
				SetDMXTestMode();
			}
		}
		$(document).ready(function () {

			$.ajax({
				url: 'api/models',
				type: 'GET',
				async: true,
				dataType: 'json',
				success: function (data) {
					modelInfos = data;

					for (var i = 0; i < modelInfos.length; i++) {
						if (modelInfos[i].StartChannel > 0) {
							modelInfos[i].EndChannel = modelInfos[i].StartChannel + modelInfos[i].ChannelCount - 1;
							modelInfos[i].ChannelsPerString = parseInt(modelInfos[i].ChannelCount / modelInfos[i].StringCount);
							// Node width drives the stride for every test pattern; fppd
							// derives the model's channel order from this same value.
							modelInfos[i].ChannelCountPerNode = modelInfos[i].ChannelCountPerNode || 3;
							var option = "<option value='" + i + "'>" + modelInfos[i].Name + "</option>\n";
							$('#modelName').append(option);
							// Populate the Channel Fader model dropdown too, but
							// limit it to fixture-sized models (<= 512ch) so
							// users don't accidentally pick a giant matrix.
							if (modelInfos[i].ChannelCount <= 512) {
								var dmxLabel = modelInfos[i].Name +
									' (Ch ' + modelInfos[i].StartChannel +
									', ' + modelInfos[i].ChannelCount + 'ch)';
								var dmxOption = $('<option></option>')
									.attr('value', i)
									.text(dmxLabel);
								$('#dmxModelName').append(dmxOption);
							}
						}
					}
					// Re-render the Channel Fader so fixture groupings from
					// model-overlays.json are highlighted on the slider grid.
					if (typeof RebuildDMXSliders === 'function') {
						RebuildDMXSliders();
					}
				},
				error: function () {
					$.jGrowl('Error: Unable to get list of models', { themeState: 'danger' });
				}
			});

			$('#testModeCycleMS').on('input', function () {
				testModeTimerInterval = $('#testModeCycleMS').val();
				$('#testModeCycleMSText').html(testModeTimerInterval);
			}).on('change', function () {
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
			$('#testModeColorS').on('input', function () {
				testModeColorS = $('#testModeColorS').val();
				$('#testModeColorSText').html(testModeColorS);
			}).on('change', function () {
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
			$('#testModeColorR').on('input', function () {
				UpdateTestModeFillColors();
			}).on('change', function () {
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

			$('#testModeColorG').on('input', function () {
				UpdateTestModeFillColors();
			}).on('change', function () {
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
			$('#testModeColorB').on('input', function () {
				UpdateTestModeFillColors();
			}).on('change', function () {
				UpdateTestModeFillColors();
				SetTestMode();
			});

			$('#testModeColorW').on('input', function () {
				UpdateTestModeFillColorW();
			}).on('change', function () {
				UpdateTestModeFillColorW();
				SetTestMode();
			});

			$('.color-box').colpick({
				layout: 'rgbhex',
				color: 'ff00ff',
				submit: false,
				onChange: function (hsb, hex, rgb, el, bySetColor) {
					$(el).css('background-color', '#' + hex);
					if (!bySetColor) {
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
			}).on("keyup", function () {
				$(this).colpickSetColor(this.value);
			})
				.css('background-color', '#ff00ff');

			// Switching tabs doesn't stop the running test, so re-read its state to
			// keep the checkbox on the newly shown tab honest.
			$('#tab-channels-tab, #tab-dmx-tab, #tab-sequence-tab').on('shown.bs.tab', function () {
				RefreshTestState();
			});

			UpdateWhiteSliderState();
			GetTestMode();
			RebuildDMXSliders();
		});

	</script>

	<style>
		#testModeColorR::-webkit-slider-thumb {
			background-color: #FF0000;
		}

		#testModeColorG::-webkit-slider-thumb {
			background-color: #00FF00;
		}

		#testModeColorB::-webkit-slider-thumb {
			background-color: #0000FF;
		}

		#testModeColorW::-webkit-slider-thumb {
			background-color: #FFFFFF;
		}

		#testModeColorR::-moz-range-thumb {
			background-color: #FF0000;
		}

		#testModeColorG::-moz-range-thumb {
			background-color: #00FF00;
		}

		#testModeColorB::-moz-range-thumb {
			background-color: #0000FF;
		}

		.dmxSliderGrid {
			margin-top: 10px;
			display: flex;
			flex-wrap: wrap;
			gap: 8px;
		}

		.dmxChannelBoxFixture {
			border-width: 2px !important;
			border-style: solid !important;
			border-top-width: 6px !important;
		}

		/* When a fixture spans multiple adjacent boxes on the same row, drop
		   the gap and the dividing borders so the coloured top edge reads as
		   a single solid bar running across the whole channel group. */
		.dmxChannelBoxFixtureCont {
			margin-left: -8px;
			border-left-width: 0 !important;
			border-top-left-radius: 0 !important;
			border-bottom-left-radius: 0 !important;
		}

		.dmxChannelBoxFixture:has(+ .dmxChannelBoxFixtureCont) {
			border-right-width: 0 !important;
			border-top-right-radius: 0 !important;
			border-bottom-right-radius: 0 !important;
		}

		.dmxFixtureName {
			font-size: 0.8em;
			font-weight: 600;
			color: inherit;
			white-space: nowrap;
			overflow: visible;
			margin-bottom: 2px;
			position: relative;
			z-index: 10;
		}

		.dmxFixtureNameSpacer {
			visibility: hidden;
		}

		.dmxRelChannel {
			font-size: 0.78em;
			color: inherit;
			margin-bottom: 4px;
		}

		.dmxChannelBox {
			flex: 0 0 auto;
			width: 90px;
			text-align: center;
			padding: 10px 6px;
			border: 1px solid rgba(128, 128, 128, 0.35);
			border-radius: 6px;
			background-color: rgba(0, 0, 0, 0.04);
			box-sizing: border-box;
		}

		.dmxChannelLabel {
			margin-bottom: 6px;
			font-size: 0.9em;
		}

		[data-bs-theme="dark"] .dmxChannelBox:not(.dmxChannelBoxFixture) .dmxChannelLabel {
			color: var(--bs-body-color);
		}

		.dmxSliderWrapper {
			position: relative;
			width: 100%;
			height: 240px;
			display: block;
			overflow: visible;
		}

		.dmxSlider.dmxSlider {
			position: absolute;
			top: 50%;
			left: 50%;
			width: 220px !important;
			height: 24px;
			padding: 0;
			margin: 0;
			flex: 0 0 auto;
			transform: translate(-50%, -50%) rotate(-90deg);
			transform-origin: center center;
		}

		.dmxValueInput {
			width: 70px;
			margin: 8px auto 0 auto;
			text-align: center;
		}

		#testModeColorW::-moz-range-thumb {
			background-color: #FFFFFF;
		}
	</style>

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
								<a class="nav-link active" id="tab-channels-tab" data-bs-toggle="tab"
									data-bs-target="#tab-channels" href="#tab-channels" role="tab"
									aria-controls="tab-channels" aria-selected="true">
									Channel Testing
								</a>
							</li>
							<li class="nav-item">
								<a class="nav-link" id="tab-dmx-tab" data-bs-toggle="tab" data-bs-target="#tab-dmx"
									href="#tab-dmx" role="tab" aria-controls="tab-dmx" aria-selected="false">
									Channel Fader
								</a>
							</li>
							<?php if (isset($settings['fppMode']) && ($settings['fppMode'] == 'player')) { ?>
								<li class="nav-item">
									<a class="nav-link" id="tab-sequence-tab" data-bs-toggle="tab"
										data-bs-target="#tab-sequence" href="#tab-sequence" role="tab"
										aria-controls="tab-sequence" aria-selected="true">
										Sequence
									</a>
								</li>
							<?php } ?>
						</ul>

						<div class="tab-content">
							<div id='tab-channels' class="tab-pane fade show active" role="tabpanel"
								aria-labelledby="interface-settings-tab">


								<div id='channelTestTakeover' class="alert alert-warning d-none d-flex flex-wrap align-items-center gap-2"
									role="alert">
									<i class="fas fa-exclamation-triangle"></i>
									<span>A test started on the <b>Channel Fader</b> tab is still running. Enabling
										test mode here will take it over.</span>
									<button type="button" class="btn btn-sm btn-outline-secondary ms-auto"
										onclick="StopRunningTest();">Stop Test</button>
								</div>
								<!-- Page-Wide Settings -->
								<div class="backdrop-dark mb-3">
									<div class="row">
										<div class="col-md-6">
											<label for="testModeEnabled" class="mb-0 d-block">
												<div><b>Enable Test Mode:</b>&nbsp;<input type='checkbox' class="ms-1"
														id='testModeEnabled' onClick='SetTestMode();'></div>
												<div><b>Multisync:</b>&nbsp;<input type='checkbox' class="ms-1"
														id='multisyncEnabled' onClick='SetTestMode();'></div>
											</label>
										</div>
										<div class="col-md-6">
											<div><b>Update Interval:</b>&nbsp;<small class="text-muted"><span
														id='testModeCycleMSText'>1000</span><span> ms</span></small>
											</div>
											<div>
												<input id="testModeCycleMS" type="range" min="100" max="5000"
													value="1000" step="100" />
											</div>
										</div>
									</div>
								</div>

								<div class="row">
									<div class="col-md-3">

										<!-- Model Testing Section -->
										<div class="backdrop-dark">
											<h4 class="mb-3">Model Testing</h4>
											<div class="form-group">
												<label for="modelName"><b>Model Name:</b></label>
												<select class='form-control' onChange='UpdateStartEndFromModel();'
													id='modelName'>
													<option value='1,<?= $testEndChannel ?>'>-- All Local Channels --
													</option>
													<option value='1,<?= FPPD_MAX_CHANNELS ?>'>-- All Channels --
													</option>
												</select>
											</div>

											<div class='row stringRow' style='display: none;'>
												<div class="col-12 mb-2"><b>String Range:</b></div>
												<div class="col-6 form-group">
													<label for="startString">Start String:</label>
													<input class="form-control" type='number' min='1' max='1' value='1'
														id='startString' onChange='StringsChanged();'
														onkeypress='this.onchange();' onpaste='this.onchange();'
														oninput='this.onchange();'>
												</div>
												<div class="col-6 form-group">
													<label for="endString">End String:</label>
													<input class="form-control" type='number' min='1' max='1' value='1'
														id='endString' onChange='StringsChanged();'
														onkeypress='this.onchange();' onpaste='this.onchange();'
														oninput='this.onchange();'>
												</div>
											</div>
											<div class='row stringRow' style='display: none;'>
												<div class="col-6 form-group">
													<input type='button' class='buttons' value='-1'
														onClick='AdjustStartString(-1);'>
													<input type='button' class='buttons' value='+1'
														onClick='AdjustStartString(1);'>
												</div>
												<div class="col-6 form-group">
													<input type='button' class='buttons' value='-1'
														onClick='AdjustEndString(-1);'>
													<input type='button' class='buttons' value='+1'
														onClick='AdjustEndString(1);'>
												</div>
											</div>
										</div>

										<!-- Channel Testing Section -->
										<div class="backdrop-dark mt-3">
											<h4 class="mb-3">Channel Testing</h4>

											<div class="mb-2"><b>Channel Range</b><small
													class="form-text text-muted">(1-<? echo FPPD_MAX_CHANNELS; ?>)
												</small></div>

											<div class="row">
												<div class="col-6 form-group">
													<label for="testModeStartChannel">Start:</label>
													<input class="form-control" type='number' min='1'
														max='<? echo FPPD_MAX_CHANNELS; ?>'
														value='<?= $testStartChannel ?>' id='testModeStartChannel'
														onChange='SetTestMode();' onkeypress='this.onchange();'
														onpaste='this.onchange();' oninput='this.onchange();'>
												</div>
												<div class="col-6 form-group">
													<label for="testModeEndChannel">End:</label>
													<input class="form-control" type='number' min='1'
														max='<? echo FPPD_MAX_CHANNELS; ?>'
														value='<?= $testEndChannel ?>' id='testModeEndChannel'
														onChange='SetTestMode();' onkeypress='this.onchange();'
														onpaste='this.onchange();' oninput='this.onchange();'>
												</div>
											</div>

											<div class="form-group mt-2">
												<label for="channelsPerPixel"><b>Channels per Pixel:</b></label>
												<select id='channelsPerPixel' class='form-control'
													title='How many channels one pixel occupies. The hardware colour order (GRB, BGR, ...) is set on the output, not here.'
													onChange='UpdateIncrementFromChannelsPerPixel(); SetTestMode();'>
													<option value='1'>1 &mdash; Single Color</option>
													<option value='3' selected>3 &mdash; RGB</option>
													<option value='4'>4 &mdash; RGBW</option>
												</select>
												<div id='strideNotice' class='form-text text-warning'
													style='display: none;'></div>
											</div>

											<div class="mb-2 mt-3"><b>Adjust Channels</b></div>
											<div class='row'>
												<div class="col-6 form-group">
													<label for='channelIncrement'>Increment:</label>
												</div>
												<div class="col-6 form-group">
													<input class="form-control" type='number' min='1'
														max='<? echo FPPD_MAX_CHANNELS; ?>' value='3'
														id='channelIncrement' onChange='SetButtonIncrements();'
														onkeypress='this.onchange();' onpaste='this.onchange();'
														oninput='this.onchange();'>
												</div>
											</div>

											<div class='row'>
												<div class="col-6 form-group">
													<label>Start:</label>
												</div>
												<div class="col-6 form-group">
													<input type='button' class='buttons' value='-3' id='decStartButton'
														onClick='adjustStartChannel(-1);'>
													<input type='button' class='buttons' value='+3' id='incStartButton'
														onClick='adjustStartChannel(1);'>
												</div>
											</div>
											<div class='row'>
												<div class="col-6 form-group">
													<label>End:</label>
												</div>
												<div class="col-6 form-group">
													<input type='button' class='buttons' value='-3' id='decEndButton'
														onClick='adjustEndChannel(-1);'>
													<input type='button' class='buttons' value='+3' id='incEndButton'
														onClick='adjustEndChannel(1);'>
												</div>
											</div>
											<div class='row'>
												<div class="col-6 form-group">
													<label>Both:</label>
												</div>
												<div class="col-6 form-group">
													<input type='button' class='buttons' value='-3' id='decBothButton'
														onClick='adjustBothChannels(-1);'>
													<input type='button' class='buttons' value='+3' id='incBothButton'
														onClick='adjustBothChannels(1);'>
												</div>
											</div>
										</div>

									</div>
									<div class="col-md-9">

										<!-- Test Pattern Tabs -->
										<ul class="nav nav-pills mb-3" role="tablist">
											<li class="nav-item">
												<a class="nav-link active" id="rgb-patterns-tab" data-bs-toggle="tab"
													data-bs-target="#rgb-patterns" href="#rgb-patterns" role="tab"
													aria-controls="rgb-patterns" aria-selected="true">
													RGB Test Patterns
												</a>
											</li>
											<li class="nav-item">
												<a class="nav-link" id="solid-color-tab" data-bs-toggle="tab"
													data-bs-target="#solid-color" href="#solid-color" role="tab"
													aria-controls="solid-color" aria-selected="false">
													Solid Color Fill
												</a>
											</li>
											<li class="nav-item">
												<a class="nav-link" id="single-channel-tab" data-bs-toggle="tab"
													data-bs-target="#single-channel" href="#single-channel" role="tab"
													aria-controls="single-channel" aria-selected="false">
													Single Channel
												</a>
											</li>
										</ul>

										<div class="tab-content">
											<!-- RGB Test Patterns Tab -->
											<div class="tab-pane fade show active" id="rgb-patterns" role="tabpanel"
												aria-labelledby="rgb-patterns-tab">
												<div class="callout callout-primary">
													<p><b>Note:</b> Selecting a Pixel Overlay Model fills in its channel
														range and automatically selects RGB or RGBW to match the model's
														pixels. Outputs configured in FPP apply the physical wire color
														order themselves, so test patterns display true colors on those
														pixels — use the R-G-B-W patterns for 4-channel RGBW pixels. For
														manual channel ranges driving hardware that FPP does not reorder
														colors for, the Color Order dropdown on the left remaps the
														Solid Color fill and sets the 3- or 4-channel pixel size.</p>
												</div>
												<div class="row">
													<div class="col-md-6">
														<div class="backdrop">
															<h3>Chase Patterns</h3>
															<h5 class="mt-2 mb-2">3-Channel (RGB)</h5>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGB'
																	id='RGBChase-RGB' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGB'>Chase:
																	R-G-B</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBA'
																	id='RGBChase-RGBA' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBA'>Chase:
																	R-G-B-All</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBN'
																	id='RGBChase-RGBN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBN'>Chase:
																	R-G-B-None</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBAN'
																	id='RGBChase-RGBAN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBAN'>Chase:
																	R-G-B-All-None</label></div>
															<h5 class="mt-3 mb-2">4-Channel (RGBW)</h5>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBW'
																	id='RGBChase-RGBW' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBW'>Chase:
																	R-G-B-W</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBWA'
																	id='RGBChase-RGBWA' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBWA'>Chase:
																	R-G-B-W-All</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBWN'
																	id='RGBChase-RGBWN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBWN'>Chase:
																	R-G-B-W-None</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBWAN'
																	id='RGBChase-RGBWAN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBWAN'>Chase:
																	R-G-B-W-All-None</label></div>
															<h5 class="mt-3 mb-2">Custom</h5>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBChase-RGBCustom'
																	id='RGBChase-RGBCustom'
																	onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBChase-RGBCustom'>Chase:
																	Custom Pattern: </label></div>
															<div class="form-group">
																<div class="mb-2">
																	<label for="customChasePatternColorPicker"
																		class="form-label"><b>Add Color to
																			Pattern:</b></label>
																	<div class="input-group" style="max-width: 300px;">
																		<input type="color"
																			class="form-control form-control-color"
																			id="customChasePatternColorPicker"
																			value="#ff0000" title="Choose color">
																		<button type="button"
																			class="btn btn-outline-secondary btn-sm"
																			onclick="AppendColorPickerToCustomChase();">Add
																			Color</button>
																		<button type="button"
																			class="btn btn-outline-danger btn-sm"
																			onclick="ClearCustomChasePattern();">Clear</button>
																	</div>
																</div>

																<input id='testModeRGBCustomPattern' size='36'
																	maxlength='72' type="text"
																	value='FF000000FF000000FF' onChange='SetTestMode();'
																	onkeypress='this.onchange();'
																	onpaste='this.onchange();'
																	oninput='this.onchange();'>
																<small class="form-text text-muted">(6 hex digits per
																	RGB
																	pixel, 8 per RGBW pixel)</small>
															</div>

														</div>
													</div>
													<div class="col-md-6">
														<div class="backdrop ">
															<h3>Cycle Patterns</h3>
															<h5 class="mt-2 mb-2">3-Channel (RGB)</h5>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGB'
																	id='RGBCycle-RGB' checked onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGB'>Cycle:
																	R-G-B</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBA'
																	id='RGBCycle-RGBA' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBA'>Cycle:
																	R-G-B-All</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBN'
																	id='RGBCycle-RGBN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBN'>Cycle:
																	R-G-B-None</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBAN'
																	id='RGBCycle-RGBAN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBAN'>Cycle:
																	R-G-B-All-None</label></div>
															<h5 class="mt-3 mb-2">4-Channel (RGBW)</h5>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBW'
																	id='RGBCycle-RGBW' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBW'>Cycle:
																	R-G-B-W</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBWA'
																	id='RGBCycle-RGBWA' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBWA'>Cycle:
																	R-G-B-W-All</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBWN'
																	id='RGBCycle-RGBWN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBWN'>Cycle:
																	R-G-B-W-None</label></div>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBWAN'
																	id='RGBCycle-RGBWAN' onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBWAN'>Cycle:
																	R-G-B-W-All-None</label></div>
															<h5 class="mt-3 mb-2">Custom</h5>
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input type='radio' class="custom-control-input"
																	name='testModeMode' value='RGBCycle-RGBCustom'
																	id='RGBCycle-RGBCustom'
																	onChange='SetTestMode();'><label
																	class="custom-control-label"
																	for='RGBCycle-RGBCustom'>Cycle:
																	Custom Pattern: </label> </div>

															<div class="form-group">
																<div class="mb-2">
																	<label for="customPatternColorPicker"
																		class="form-label"><b>Add Color to
																			Pattern:</b></label>
																	<div class="input-group" style="max-width: 300px;">
																		<input type="color"
																			class="form-control form-control-color"
																			id="customPatternColorPicker"
																			value="#ff0000" title="Choose color">
																		<button type="button"
																			class="btn btn-outline-secondary"
																			onclick="AppendColorPickerToCustom();">Add
																			Color</button>
																		<button type="button"
																			class="btn btn-outline-danger"
																			onclick="ClearCustomPattern();">Clear</button>
																	</div>
																</div>

																<input id='testModeRGBCycleCustomPattern' size='36'
																	maxlength='72' type="text"
																	value='FF000000FF000000FF' onChange='SetTestMode();'
																	onkeypress='this.onchange();'
																	onpaste='this.onchange();'
																	oninput='this.onchange();'>
																<small class="form-text text-muted">(6 hex digits per
																	RGB
																	pixel, 8 per RGBW pixel) </small>
															</div>
														</div>
													</div>
												</div>
											</div>

											<!-- Solid Color Tab -->
											<div class="tab-pane fade" id="solid-color" role="tabpanel"
												aria-labelledby="solid-color-tab">
												<div class="callout callout-primary">
													<p><b>Note:</b> Select a specific color to fill all channels
														(respects Color Order setting above).</p>
												</div>
												<div class="backdrop mt-3">
													<div class="row">
														<div class="col-auto displayTestingFillOptionBoxHeader">
															<div
																class="testPatternOptionRow custom-control custom-radio">
																<input class="custom-control-input" type='radio'
																	name='testModeMode' value='RGBFill' id="RGBFill"
																	onChange='SetTestMode();'>
																<label for="RGBFill" class="custom-control-label">
																	<h3>Fill Color:</h3>
																</label>
															</div>
															<div class="color-box"></div>
														</div>
														<div class="col-auto ms-auto">
															<input type=button class="buttons"
																onClick='AppendFillToCustom();'
																value='Append Color To Custom Pattern'>

														</div>
													</div>
													<div class="row">

														<div class="col-sm-3 testModeColorRange"><span>R: </span><input
																id="testModeColorR" type="range" min="0" max="255"
																value="255" step="1" /> <span
																id='testModeColorRText'>255</span><span></span></div>
														<div class="col-sm-3 testModeColorRange"><span>G: </span><input
																id="testModeColorG" type="range" min="0" max="255"
																value="0" step="1" /> </span> <span
																id='testModeColorGText'>0</span><span></span></div>
														<div class="col-sm-3 testModeColorRange"><span>B: </span><input
																id="testModeColorB" type="range" min="0" max="255"
																value="255" step="1" /> <span
																id='testModeColorBText'>255</span><span></span></div>
														<div class="col-sm-3 testModeColorRange"><span>W: </span><input
																id="testModeColorW" type="range" min="0" max="255"
																value="0" step="1" /> <span
																id='testModeColorWText'>0</span><span></span></div>

													</div>
												</div>
											</div>

											<!-- Single Channel Tab -->
											<div class="tab-pane fade" id="single-channel" role="tabpanel"
												aria-labelledby="single-channel-tab">
												<div class="callout callout-primary">
													<p><b>Note:</b> Test individual channels with chase or fill
														patterns.</p>
												</div>
												<div class="backdrop">
													<span><b>&nbsp;Channel Data Value: </b></span>

													<div><input id="testModeColorS" type="range" min="0" max="255"
															value="255" step="1" /> </div>

													<div><span id='testModeColorSText'>255</span></div>


													<div class="row">
														<div class="col-auto">
															<div class="form-row">
																<div
																	class="testChannelOptionRow custom-control custom-radio">
																	<input class="custom-control-input" type='radio'
																		name='testModeMode' value='SingleChase'
																		id='SingleChase' onChange='SetTestMode();'>
																	<label for="SingleChase"
																		class="custom-control-label"><b>Chase</b></label>
																</div>
																<div class="form-col ms-2 pt-1">

																	Chase Size:
																	<select id='testModeChaseSize'
																		onChange='SetTestMode();'>
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
															<div
																class="testChannelOptionRow custom-control custom-radio">
																<input class="custom-control-input" type='radio'
																	name='testModeMode' value='SingleFill'
																	id='SingleFill' onChange='SetTestMode();'>
																<label for="SingleFill"
																	class="custom-control-label"><b>Fill</b></label>
															</div>
														</div>
													</div>
												</div>
											</div>



										</div>
									</div>
								</div>
							</div>
							<div id='tab-dmx' class="tab-pane fade" role="tabpanel" aria-labelledby="tab-dmx-tab">
								<div id='dmxTestTakeover' class="alert alert-warning d-none d-flex flex-wrap align-items-center gap-2"
									role="alert">
									<i class="fas fa-exclamation-triangle"></i>
									<span>A test started on the <b>Channel Testing</b> tab is still running. Enabling
										test mode here will take it over.</span>
									<button type="button" class="btn btn-sm btn-outline-secondary ms-auto"
										onclick="StopRunningTest();">Stop Test</button>
								</div>
								<div class="row">
									<div class="col-md-3">
										<div class="backdrop-dark">
											<label for="dmxTestEnabled" class="mb-0 d-block">
												<div><b>Enable Test Mode:</b>&nbsp;<input type='checkbox' class="ms-1"
														id='dmxTestEnabled' onClick='SetDMXTestMode();'></div>
												<div><b>Multisync:</b>&nbsp;<input type='checkbox' class="ms-1"
														id='dmxMultisyncEnabled' onClick='SetDMXTestMode();'></div>
											</label>
										</div>
										<div class="backdrop-dark mt-3">
											<div class="form-group">
												<label for='dmxModelName'><b>Model:</b></label>
												<select class="form-control" id='dmxModelName'
													onChange='DMXUpdateFromModel();'>
													<option value=''>-- Manual --</option>
												</select>
												<small class="form-text text-muted">Selecting a model fills in
													the start channel and channel count below.</small>
											</div>
											<div class="form-group mt-2">
												<label for='dmxStartChannel'><b>Start Channel:</b></label>
												<input class="form-control" type='number' min='1'
													max='<? echo FPPD_MAX_CHANNELS; ?>' value='1' id='dmxStartChannel'
													onChange="$('#dmxModelName').val(''); RebuildDMXSliders();">
											</div>
											<div class="form-group mt-2">
												<label for='dmxChannelCount'><b>Number of Channels:</b></label>
												<input class="form-control" type='number' min='1' max='512' value='16'
													id='dmxChannelCount'
													onChange="$('#dmxModelName').val(''); RebuildDMXSliders();">
												<small class="form-text text-muted">Channels are grouped into tabs of
													16</small>
											</div>
										</div>
										<div class="backdrop-dark mt-3">
											<div><b>Quick Set</b></div>
											<div class="mt-2">
												<input type='button' class='buttons' value='All 0'
													onClick='DMXSetAll(0);'>
												<input type='button' class='buttons' value='All 128'
													onClick='DMXSetAll(128);'>
												<input type='button' class='buttons' value='All 255'
													onClick='DMXSetAll(255);'>
											</div>
											<hr class="my-2">
											<div><b>Sine Wave</b></div>
											<div class="form-group mt-1">
												<label for='dmxSineSpeed'>Speed (cycles/sec):</label>
												<input class="form-control" type='number' min='0.1' max='10' step='0.1'
													value='0.5' id='dmxSineSpeed'>
											</div>
											<div class="form-group mt-1">
												<label for='dmxSineSpread'>Channel Spread:</label>
												<input class="form-control" type='number' min='0' max='10' step='0.1'
													value='1' id='dmxSineSpread'>
												<small class="form-text text-muted">0 = all channels in phase; higher
													spreads the wave across channels</small>
											</div>
											<div class="mt-2">
												<input type='button' class='buttons' value='Start Sine'
													id='dmxSineStartBtn' onClick='DMXSineStart();'>
												<input type='button' class='buttons' value='Stop Sine'
													id='dmxSineStopBtn' onClick='DMXSineStop();' disabled>
											</div>
										</div>
									</div>
									<div class="col-md-9">
										<h2>Channel Fader</h2>
										<div class="callout callout-primary">
											<p><b>Note:</b> Use this tool to control individual channels for
												troubleshooting moving heads, dumb RGB fixtures, fog machines,
												pixels, and other devices. Adjust each slider (0-255) to set the
												value for that channel. Channel numbers shown are absolute FPP
												channel numbers starting at the configured Start Channel. Fixtures
												defined in <i>Input/Output Setup->Pixel Overlay Models</i> are
												highlighted with a
												coloured bar and show their name plus the channel position within
												the fixture (e.g. <i>Ch&nbsp;3 / 16</i>) so you can identify which
												function a given absolute channel maps to.
											</p>
										</div>
										<ul class="nav nav-pills pageContent-tabs" id="dmxSliderTabsNav" role="tablist">
										</ul>
										<div class="tab-content" id="dmxSliderTabsContent"></div>
									</div>
								</div>
							</div>
							<div id='tab-sequence' class="tab-pane fade" role="tabpanel"
								aria-labelledby="interface-settings-tab">
								<div>
									<div>
										<table border='0' cellspacing='3'>
											<tr>
												<td>Sequence:</td>
												<td><?php PrintSequenceOptions(); ?></td>
											</tr>
											<tr>
												<td>Start Time:</td>
												<td><input type='text' size='4' maxlength='4' value='0'
														id='startSecond'> (Seconds from beginning of sequence)</td>
											</tr>

											<tr>
												<td><input type='button' class="buttons" value='Play'
														onClick='PlaySequence();' id='playSequence'><input type='button'
														class="buttons" value='Stop' onClick='StopSequence();'
														id='stopSequence'></td>
												<td>Play/stop the selected sequence</td>
											</tr>
											<tr>
												<td><input type='button' class="buttons" value='Pause/UnPause'
														onClick='ToggleSequencePause();'></td>
												<td>Pause a running sequence or UnPause a paused sequence</td>
											</tr>
											<tr>
												<td><input type='button' class="buttons" value='Step'
														onClick='SingleStepSequence();'></td>
												<td>Single-step a paused sequence one frame</td>
											</tr>
										</table>
										<br>
										<div class="callout">
											<h4>Sequence Testing Limitations:</h4>
											<ol>
												<li>This page is for testing sequences, it does not test audio or video
													or synchronization of a sequence with any media file. It does test
													Master/Remote sequence synchronization.</li>
												<li>The Sequence Testing functionality currently only works when FPP is
													in an idle state and no playlists are playing. If a playlist starts
													while testing a sequence, the sequence being tested will be stopped
													automatically.</li>
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