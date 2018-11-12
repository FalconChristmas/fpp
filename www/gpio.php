<!DOCTYPE html>
<html>
<?php
require_once('config.php');
require_once("common.php");

?>
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
    if ($settings['Platform'] == "Raspberry Pi") {
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
        if (isset($settings['PiFaceDetected']) && ($settings['PiFaceDetected'] == "1"))
            $piFaceStyle = "";

        foreach ($piFaceInputs as $input)
        {
            $inputData = explode(":", $input);
            $settingID = sprintf("GPIOInput%03dEnabled", $inputData[1]);

            if (isset($settings[$settingID]) && ($settings[$settingID] == 1))
                $piFaceStyle = "";
        }
    
        $inputs = array(
                    "BCM 4:4:7:P1 - Pin 7",
                    "BCM 17:17:0:P1 - Pin 11",
                    "BCM 18:18:1:P1 - Pin 12",
                    "BCM 27:27:2:P1 - Pin 13",
                    "BCM 22:22:3:P1 - Pin 15",
                    "BCM 23:23:4:P1 - Pin 16",
                    "BCM 24:24:5:P1 - Pin 18",
                    $piFaceStyle == ""
                    ? "BCM&nbsp;25&nbsp;**:25:6:P1 - Pin 22"
                    : "BCM 25:25:6:P1 - Pin 22",
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
    } else if ($settings['Platform'] == "BeagleBone Black") {
        $piFaceStyle = "";
        $piFaceInputs = array( );
        if (strpos($settings['SubPlatform'], 'PocketBeagle') !== FALSE) {
            $inputs = array(
                            "gpio2_23:87:87:P1-02",
                            "gpio2_25:89:89:P1-04",
                            "gpio0_5:5:5:P1-06",
                            "gpio0_2:2:2:P1-08",
                            "gpio0_3:3:3:P1-10",
                            "gpio0_4:4:4:P1-12",
                            "gpio0_20:20:20:P1-20",
                            "gpio0_12:12:12:P1-26",
                            "gpio0_13:13:13:P1-28",
                            "gpio3_21:117:117:P1-29",
                            "gpio1_11:43:43:P1-30",
                            "gpio3_18:114:114:P1-31",
                            "gpio1_10:42:42:P1-32",
                            "gpio3_15:111:111:P1-33",
                            "gpio0_26:26:26:P1-34",
                            "gpio2_24:88:88:P1-35",
                            "gpio3_14:110:110:P1-36",
                            "gpio1_18:50:50:P2-01",
                            "gpio1_27:59:59:P2-02",
                            "gpio0_23:23:23:P2-03",
                            "gpio1_26:58:58:P2-04",
                            "gpio0_30:30:30:P2-05",
                            "gpio1_25:57:57:P2-06",
                            "gpio0_31:31:31:P2-07",
                            "gpio1_28:60:60:P2-08",
                            "gpio0_15:15:15:P2-09",
                            "gpio1_20:52:52:P2-10",
                            "gpio0_14:14:14:P2-11",
                            "gpio2_1:65:65:P2-17",
                            "gpio1_15:47:47:P2-18",
                            "gpio0_27:27:27:P2-19",
                            "gpio2_0:64:64:P2-20",
                            "gpio1_14:46:46:P2-22",
                            "gpio1_12:44:44:P2-24",
                            "gpio1_9:41:41:P2-25",
                            "gpio1_8:40:40:P2-27",
                            "gpio3_20:116:116:P2-28",
                            "gpio0_7:7:7:P2-29",
                            "gpio3_17:113:113:P2-30",
                            "gpio0_19:19:19:P2-31",
                            "gpio3_16:112:112:P2-32",
                            "gpio1_13:45:45:P2-33",
                            "gpio3_19:115:115:P2-34",
                            "gpio2_22:86:86:P2-35"
                            );
        } else {
            $inputs = array(
                            "gpio1_6:38:38:P8-03",
                            "gpio1_7:39:39:P8-04",
                            "gpio1_2:34:34:P8-05",
                            "gpio1_3:35:35:P8-06",
                            "gpio2_2:66:66:P8-07",
                            "gpio2_3:67:67:P8-08",
                            "gpio2_5:69:69:P8-09",
                            "gpio2_4:68:68:P8-10",
                            "gpio1_13:45:45:P8-11",
                            "gpio1_12:44:44:P8-12",
                            "gpio0_23:23:23:P8-13",
                            "gpio0_26:26:26:P8-14",
                            "gpio1_15:47:47:P8-15",
                            "gpio1_14:46:46:P8-16",
                            "gpio0_27:27:27:P8-17",
                            "gpio2_1:65:65:P8-18",
                            "gpio0_22:22:22:P8-19",
                            "gpio1_31:63:63:P8-20",
                            "gpio1_30:62:62:P8-21",
                            "gpio1_5:37:37:P8-22",
                            "gpio1_4:36:36:P8-23",
                            "gpio1_1:33:33:P8-24",
                            "gpio1_0:32:32:P8-25",
                            "gpio1_29:61:61:P8-26",
                            "gpio2_22:86:86:P8-27",
                            "gpio2_24:88:88:P8-28",
                            "gpio2_23:87:87:P8-29",
                            "gpio2_25:89:89:P8-30",
                            "gpio0_10:10:10:P8-31",
                            "gpio0_11:11:11:P8-32",
                            "gpio0_9:9:9:P8-33",
                            "gpio2_17:81:81:P8-34",
                            "gpio0_8:8:8:P8-35",
                            "gpio2_16:80:80:P8-36",
                            "gpio2_14:78:78:P8-37",
                            "gpio2_15:79:79:P8-38",
                            "gpio2_12:76:76:P8-39",
                            "gpio2_13:77:77:P8-40",
                            "gpio2_10:74:74:P8-41",
                            "gpio2_11:75:75:P8-42",
                            "gpio2_8:72:72:P8-43",
                            "gpio2_9:73:73:P8-44",
                            "gpio2_6:70:70:P8-45",
                            "gpio2_7:71:71:P8-46",
                            "gpio0_30:30:30:P9-11",
                            "gpio1_28:60:60:P9-12",
                            "gpio0_31:31:31:P9-13",
                            "gpio1_18:50:50:P9-14",
                            "gpio1_16:48:48:P9-15",
                            "gpio1_19:51:51:P9-16",
                            "gpio0_5:5:5:P9-17",
                            "gpio0_4:4:4:P9-18",
                            "gpio0_13:13:13:P9-19",
                            "gpio0_12:12:12:P9-20",
                            "gpio0_3:3:3:P9-21",
                            "gpio0_2:2:2:P9-22",
                            "gpio1_17:49:49:P9-23",
                            "gpio0_15:15:15:P9-24",
                            "gpio3_21:117:117:P9-25",
                            "gpio0_14:14:14:P9-26",
                            "gpio3_19:115:115:P9-27",
                            "gpio3_17:113:113:P9-28",
                            "gpio3_15:111:111:P9-29",
                            "gpio3_16:112:112:P9-30",
                            "gpio3_14:110:110:P9-31",
                            "gpio0_20:20:20:P9-41",
                            "gpio3_20:116:116:P9-41",
                            "gpio0_7:7:7:P9-42",
                            "gpio3_18:114:114:P9-42"
                            );
        }
    }
?>
<script language="Javascript">

function SaveGPIOInputEvent(input)
{
	var event = $(input).val();
	var setting = $(input).attr('id');

	$.get("fppjson.php?command=setSetting&key=" + setting + "&value="
		+ event)
		.done(function() {
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
				<td colspan=5 align='center'><b>Built-in GPIO Inputs</b></td>
		</tr>
		<tr class='fppTableHeader'>
				<td width='5%'>En.</td>
				<td width='12%'>GPIO #</td>
				<td width='10%'>wiring #</td>
				<td width='55%' colspan='2'>Events</td>
				<td width='18%' align='center'>Hdr&nbsp;-&nbsp;Pin</td>
		</tr>
<?

	foreach ($inputs as $input)
	{
		$inputData = explode(":", $input);
		$settingID = sprintf("GPIOInput%03dEnabled", $inputData[1]);
?>
		<tr><td><? PrintSettingCheckbox("GPIO Input", $settingID, 1, 0, "1", "0"); ?></td>
				<td><?= $inputData[0]; ?></td>
				<td><?= $inputData[2]; ?></td>
				<td>Rising: <? PrintEventOptions($inputData[1]); ?></td>
				<td>Falling: <? PrintEventOptions($inputData[1], false); ?></td>
				<td align='center'><?= $inputData[3]; ?></td>
		</tr>
<?
	}
if ($settings['Platform'] == "Raspberry Pi") {
    ?>
		<tr <? echo $piFaceStyle; ?>><td colspan=5><br>NOTE: ** - BCM 25 conflicts with PiFace GPIO Inputs, do not enable BCM 25 when using a PiFace.<br></td></tr>
		<tr class='piFaceGPIO' <? echo $piFaceStyle; ?>>
				<td colspan=6 align='center'><hr><b>PiFace GPIO Inputs</b></td>
		</tr>
		<tr class='fppTableHeader' <? echo $piFaceStyle; ?>>
				<td>En.</td>
				<td>GPIO #</td>
				<td>wiring #</td>
				<td colspan="2">Events</td>
				<td align='center'>Input #</td>
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
				<td>Rising: <? PrintEventOptions($inputData[1]); ?></td>
				<td>Falling: <? PrintEventOptions($inputData[1], false); ?></td>
				<td align='center'><?= $inputData[3]; ?></td>
		</tr>
<?
	}
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
