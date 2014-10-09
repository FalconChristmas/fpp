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

		echo "<select id='$key' onChange='SaveGPIOInputEvent(this);'>";
		echo $eventOptions;
		echo "</select>";
	}

	$piFaceStyle = "style='display: none;'";
	if (isset($settings['PiFaceDetected']) && ($settings['PiFaceDetected'] == 1))
		$piFaceStyle = "";
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
				<td colspan=6 align='center'><b>Built-in Raspberry Pi GPIO Inputs</b></td>
		</tr>
		<tr class='fppTableHeader'>
				<td width='5%'>En.</td>
				<td width='12%'>GPIO #</td>
				<td width='10%'>wiringPi #</td>
				<td width='30%'>Event Rising</td>
				<td width='31%'>Event Falling</td>
				<td width='12%'>Hdr - Pin</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput004Enabled", "1", "0"); ?></td>
				<td>BCM 4</td>
				<td>7</td>
				<td><? PrintEventOptions(4); ?></td>
				<td><? PrintEventOptions(4, false); ?></td>
				<td>P1 - Pin 7</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput017Enabled", "1", "0"); ?></td>
				<td>BCM 17</td>
				<td>0</td>
				<td><? PrintEventOptions(17); ?></td>
				<td><? PrintEventOptions(17, false); ?></td>
				<td>P1 - Pin 11</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput018Enabled", "1", "0"); ?></td>
				<td>BCM 18</td>
				<td>1</td>
				<td><? PrintEventOptions(18); ?></td>
				<td><? PrintEventOptions(18, false); ?></td>
				<td>P1 - Pin 12</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput027Enabled", "1", "0"); ?></td>
				<td>BCM 27</td>
				<td>2</td>
				<td><? PrintEventOptions(27); ?></td>
				<td><? PrintEventOptions(27, false); ?></td>
				<td>P1 - Pin 13</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput022Enabled", "1", "0"); ?></td>
				<td>BCM 22</td>
				<td>3</td>
				<td><? PrintEventOptions(22); ?></td>
				<td><? PrintEventOptions(22, false); ?></td>
				<td>P1 - Pin 15</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput023Enabled", "1", "0"); ?></td>
				<td>BCM 23</td>
				<td>4</td>
				<td><? PrintEventOptions(23); ?></td>
				<td><? PrintEventOptions(23, false); ?></td>
				<td>P1 - Pin 16</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput024Enabled", "1", "0"); ?></td>
				<td>BCM 24</td>
				<td>5</td>
				<td><? PrintEventOptions(24); ?></td>
				<td><? PrintEventOptions(24, false); ?></td>
				<td>P1 - Pin 18</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput025Enabled", "1", "0"); ?></td>
				<td>BCM 25<span <? echo $piFaceStyle; ?>> **</span></td>
				<td>6</td>
				<td><? PrintEventOptions(25); ?></td>
				<td><? PrintEventOptions(25, false); ?></td>
				<td>P1 - Pin 22</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput028Enabled", "1", "0"); ?></td>
				<td>BCM 28</td>
				<td>17</td>
				<td><? PrintEventOptions(28); ?></td>
				<td><? PrintEventOptions(28, false); ?></td>
				<td>P5 - Pin 3</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput029Enabled", "1", "0"); ?></td>
				<td>BCM 29</td>
				<td>18</td>
				<td><? PrintEventOptions(29); ?></td>
				<td><? PrintEventOptions(29, false); ?></td>
				<td>P5 - Pin 4</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput030Enabled", "1", "0"); ?></td>
				<td>BCM 30</td>
				<td>19</td>
				<td><? PrintEventOptions(30); ?></td>
				<td><? PrintEventOptions(30, false); ?></td>
				<td>P5 - Pin 5</td>
		</tr>
		<tr><td><? PrintSettingCheckbox("GPIO Input", "GPIOInput031Enabled", "1", "0"); ?></td>
				<td>BCM 31</td>
				<td>20</td>
				<td><? PrintEventOptions(31); ?></td>
				<td><? PrintEventOptions(31, false); ?></td>
				<td>P5 - Pin 6</td>
		</tr>

		<tr <? echo $piFaceStyle; ?>><td colspan=4><br>NOTE: ** - BCM 25 conflicts with PiFace GPIO Inputs, do not enable BCM 25 when using a PiFace.<br></td></tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td colspan=5 align='center'><hr><b>PiFace GPIO Inputs</b></td>
		</tr>
		<tr class='fppTableHeader' <? echo $piFaceStyle; ?>>
				<td width='5%'>En.</td>
				<td width='12%'>GPIO #</td>
				<td width='35%'>Event Rising</td>
				<td width='36%'>Event Falling</td>
				<td width='12%'>Input #</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput200Enabled", "1", "0"); ?></td>
				<td>PiFace 1</td>
				<td><? PrintEventOptions(200); ?></td>
				<td><? PrintEventOptions(200, false); ?></td>
				<td>Input 1</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput201Enabled", "1", "0"); ?></td>
				<td>PiFace 2</td>
				<td><? PrintEventOptions(201); ?></td>
				<td><? PrintEventOptions(201, false); ?></td>
				<td>Input 2</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput202Enabled", "1", "0"); ?></td>
				<td>PiFace 3</td>
				<td><? PrintEventOptions(202); ?></td>
				<td><? PrintEventOptions(202, false); ?></td>
				<td>Input 3</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput203Enabled", "1", "0"); ?></td>
				<td>PiFace 4</td>
				<td><? PrintEventOptions(203); ?></td>
				<td><? PrintEventOptions(203, false); ?></td>
				<td>Input 4</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput204Enabled", "1", "0"); ?></td>
				<td>PiFace 5</td>
				<td><? PrintEventOptions(204); ?></td>
				<td><? PrintEventOptions(204, false); ?></td>
				<td>Input 5</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput205Enabled", "1", "0"); ?></td>
				<td>PiFace 6</td>
				<td><? PrintEventOptions(205); ?></td>
				<td><? PrintEventOptions(205, false); ?></td>
				<td>Input 6</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput206Enabled", "1", "0"); ?></td>
				<td>PiFace 7</td>
				<td><? PrintEventOptions(206); ?></td>
				<td><? PrintEventOptions(206, false); ?></td>
				<td>Input 7</td>
		</tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td><? PrintSettingCheckbox("GPIO Input", "GPIOInput207Enabled", "1", "0"); ?></td>
				<td>PiFace 8</td>
				<td><? PrintEventOptions(207); ?></td>
				<td><? PrintEventOptions(207, false); ?></td>
				<td>Input 8</td>
		</tr>
	</table>

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
