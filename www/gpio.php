<?php
require_once("common.php");

?>
<html>
<head>
<?php
include 'common/menuHead.inc';

	$eventFiles = scandir($eventDirectory);

	function PrintEventOptions($gpio, $rising = true)
	{
		global $eventDirectory;
		global $eventFiles;
		global $settings;
		$eventOptions = "";
		$value = "";

		$key = 'GPIOInput' . sprintf('%03d', $gpio) . 'Event' . ( $rising ? "Rising" : "Falling" );
		if (isset($settings[$key]))
		{
			$value = $settings[$key];
		}

		$eventOptions .= "<option value=''> -- No Event -- </option>";
		foreach ($eventFiles as $eventFile)
		{
			if($eventFile != '.' && $eventFile != '..' && preg_match('/.fevt$/', $eventFile))
			{
				$info = parse_ini_file($eventDirectory . "/" . $eventFile);

				$eventFile = preg_replace('/.fevt$/', '', $eventFile);

				$eventOptions .= "<option value='$eventFile'";
				if ($value == $eventFile)
					$eventOptions .= " selected";
				$eventOptions .= ">" .
						$info['majorID'] . ' / ' . $info['minorID'] . " - " .
						$info['name'] .
						"</option>";
			}
		}

		echo "<select id='$key' onChange='SaveGPIOInputEvent(this);' style='max-width:95%'>";
		echo $eventOptions;
		echo "</select>";
	}

	$piFaceInputs = array(
		"PiFace 1:200:200:P1 - Input 1",
		"PiFace 2:201:201:P1 - Input 2",
		"PiFace 3:202:202:P1 - Input 3",
		"PiFace 4:203:203:P1 - Input 4",
		"PiFace 5:204:204:P1 - Input 5",
		"PiFace 6:205:205:P1 - Input 6",
		"PiFace 7:206:206:P1 - Input 7",
		"PiFace 8:207:207:P1 - Input 8"
		);

	$piFaceStyle = "style='display: none;'";
	if (isset($settings['PiFaceDetected']) && ($settings['PiFaceDetected'] == 1))
		$piFaceStyle = "";

	foreach ($piFaceInputs as $input)
	{
		$inputData = explode(":", $input);
		$settingID = sprintf("GPIOInput%03dEnabled", $inputData[1]);

		if (isset($settings[$settingID]) && ($settings[$settingID] == 1))
			$piFaceStyle = "";
	}


?>
<script language="Javascript">

function SaveGPIOInputEvent(input)
{
	var event = $(input).val();
	var setting = $(input).attr('id');

	$.get("fppjson.php?command=setSetting&key=" + setting + "&value="
		+ event)
		.success(function() {
			$.jGrowl('GPIO Input Event saved');
		}).fail(function() {
			DialogError("GPIO Input", "Save GPIO Event failed");
			$('#' + setting).val("");
		});
}

/////////////////////////////////////////////////////////////////////////////
$(document).ready(function(){

});

</script>

<title><? echo $pageTitle; ?></title>
</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

<div id='channelOutputManager'>
				<div id='divGPIO'>
					<fieldset class="fs">
						<legend> GPIO Input Triggers </legend>
						<div id='divGPIOData'>

<!-- --------------------------------------------------------------------- -->

  <div style="overflow: hidden; padding: 10px;">
	<table id='GPIOInputs' class='fppTable'>
		<tr>
				<td colspan=5 align='center'><b>Built-in Raspberry Pi GPIO Inputs</b></td>
		</tr>
		<tr class='fppTableHeader'>
				<td width='5%'>En.</td>
				<td width='12%'>GPIO #</td>
				<td width='10%'>wiringPi #</td>
				<td width='55%'>Events</td>
				<td width='18%'>Hdr&nbsp;-&nbsp;Pin</td>
		</tr>
<?
	$inputs = array(
		"BCM 4:4:7:P1 - Pin 7",
		"BCM 17:17:0:P1 - Pin 11",
		"BCM 18:18:1:P1 - Pin 12",
		"BCM 27:27:2:P1 - Pin 13",
		"BCM 22:22:3:P1 - Pin 15",
		"BCM 23:23:4:P1 - Pin 16",
		"BCM 24:24:5:P1 - Pin 18",
		"BCM&nbsp;25&nbsp;**:25:6:P1 - Pin 22",
		"BCM 5:5:0:P1 - Pin 29",
		"BCM 6:6:0:P1 - Pin 31",
		"BCM 12:12:0:P1 - Pin 32",
		"BCM 13:13:0:P1 - Pin 33",
		"BCM 16:16:0:P1 - Pin 36",
		"BCM 19:19:0:P1 - Pin 35",
		"BCM 20:20:0:P1 - Pin 38",
		"BCM 21:21:0:P1 - Pin 40",
		"BCM 26:26:0:P1 - Pin 37",
		"BCM 28:28:17:P5 - Pin 3",
		"BCM 29:29:18:P5 - Pin 4",
		"BCM 30:30:19:P5 - Pin 5",
		"BCM 31:31:20:P5 - Pin 6"
		);
	foreach ($inputs as $input)
	{
		$inputData = explode(":", $input);
		$settingID = sprintf("GPIOInput%03dEnabled", $inputData[1]);
?>
		<tr><td><? PrintSettingCheckbox("GPIO Input", $settingID, 1, 0, "1", "0"); ?></td>
				<td><?= $inputData[0]; ?></td>
				<td><?= $inputData[2]; ?></td>
				<td rowspan='2'>
					<table>
						<tr><td>Rising:</td><td><? PrintEventOptions($inputData[1]); ?></td></tr>
						<tr><td>Falling:</td><td><? PrintEventOptions($inputData[1], false); ?></td></tr>
					</table>
				</td>
				<td><?= $inputData[3]; ?></td>
		</tr><tr></tr>
<?
	}
?>
		<tr <? echo $piFaceStyle; ?>><td colspan=5><br>NOTE: ** - BCM 25 conflicts with PiFace GPIO Inputs, do not enable BCM 25 when using a PiFace.<br></td></tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td colspan=6 align='center'><hr><b>PiFace GPIO Inputs</b></td>
		</tr>
		<tr class='fppTableHeader' <? echo $piFaceStyle; ?>>
				<td>En.</td>
				<td>GPIO #</td>
				<td>wiringPi #</td>
				<td>Events</td>
				<td>Input #</td>
		</tr>
<?
	foreach ($piFaceInputs as $input)
	{
		$inputData = explode(":", $input);
		$settingID = sprintf("GPIOInput%03dEnabled", $inputData[1]);
?>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", $settingID, 1, 0, "1", "0"); ?></td>
				<td><?= $inputData[0]; ?></td>
				<td><?= $inputData[2]; ?></td>
				<td rowspan='2'>
					<table>
						<tr><td>Rising:</td><td><? PrintEventOptions($inputData[1]); ?></td></tr>
						<tr><td>Falling:</td><td><? PrintEventOptions($inputData[1], false); ?></td></tr>
					</table>
				</td>
				<td><?= $inputData[3]; ?></td>
		</tr><tr></tr>
<?
	}
?>
	</table>

	</div>

<!-- --------------------------------------------------------------------- -->

						</div>
					</fieldset>
				</div>
			</div>


	<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
