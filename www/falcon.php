<?php
require_once('config.php');
?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">
$(document).ready(function() {

});

function SaveHardwareConfig()
{
	var dataString = $("#frmHardware").serializeArray();
	$.ajax({
		type: "post",
		url: "fppxml.php",
		dataType:"text",
		data: dataString,
		success: function (response) {
			$.jGrowl("Hardware Config Saved");
		},
		failure: function(response) {
			DialogError("Save Hardware Config", "Save Failed!");
		}
	});
}

</script>

<?
$hwModel = "F16V2-alpha";
$hwFWVer = "1.0";

function printNodeTypeOptions($selected)
{
	$NodeTypeOptions = array("WS2811", "TM18xx", "WS2801", "TLS3001", "LPD6803",
		"LPD8806", "UCS1903", "P9813");

	$result = "";
	$i = 0;
	foreach ($NodeTypeOptions as $type)
	{
		$selectedFlag = ($selected == $i) ? "selected" : "";

		$result .= sprintf("<option value='%d' %s>%s</option>\n",
			$i, $selectedFlag, $type);
		$i++;
	}

	return $result;
}

function printRGBColorOptions($selected)
{
	$ColorOptions = array("RGB", "RBG", "GRB", "GBR", "BRG", "BGR");

	$result = "";
	$i = 0;
	foreach ($ColorOptions as $color)
	{
		$selectedFlag = ($selected == $i) ? "selected" : "";

		$result .= sprintf("<option value='%d' %s>%s</option>\n",
			$i, $selectedFlag, $color);
		$i++;
	}

	return $result;
}

function printDirectionOptions($selected)
{
	$result = "";
	$result .= sprintf("<option value='0' %s>Forward</option>\n", $selected == 0 ? "selected" : "");
	$result .= sprintf("<option value='1' %s>Reverse</option>\n", $selected == 1 ? "selected" : "");

	return $result;
}

function printFalconConfig()
{
	global $hwModel;
	global $hwFWVer;
	global $fppHome;

	if ($hwModel == "F16V2-alpha")
	{
		$cfgFile = $fppHome . '/media/config/Falcon.F16V2-alpha';
		$s = str_pad("", 1024, "\x0");
		if (file_exists($cfgFile))
		{
			$f = fopen($cfgFile, 'rb');
			$s = fread($f, 1024);
			fclose($f);
		}

		$sarr = unpack("C*", $s);

		echo "<tr class='tblheader'>
	<td width='5%'>#</td>
	<td width='10%'>Node<br>Count</td>
	<td width='10%'>Start<br>Channel</td>
	<td width='8%'>Node<br>Type</td>
	<td width='5%'>RGB<br>Order</td>
	<td width='8%'>Direction</td>
	<td width='10%'>Group<br>Count</td>
	<td width='10%'>Null<br>Nodes</td>
</tr>";

		$dataOffset = 8;

		$i = 0;
		for ($i = 0; $i < 16; $i++)
		{
			$outputOffset = $dataOffset + (9 * $i);

			$nodeCount     = $sarr[$outputOffset + 0];
			$nodeCount    += $sarr[$outputOffset + 1] * 256;

			$startChannel  = $sarr[$outputOffset + 2];
			$startChannel += $sarr[$outputOffset + 3] * 256;
			$startChannel += 1;  // 0-based values in config file

			$nodeType      = $sarr[$outputOffset + 4];
			$rgbOrder      = $sarr[$outputOffset + 5];
			$direction     = $sarr[$outputOffset + 6];
			$groupCount    = $sarr[$outputOffset + 7];
			$nullNodes     = $sarr[$outputOffset + 8];

			if ($startChannel < 1)
				$startChannel = 1;

			if ($i && ($i % 4) == 0)
				printf( "<tr name='hrRow'><td colspan='8'><hr></td></tr>\n");

			printf( "<tr id='hwRow%d'>
	<td class='center'>%d</td>
	<td class='center'><input id='nodeCount[%d]' name='nodeCount[%d]' type='text' size='3' maxlength='3' value='%d'></td>
	<td class='center'><input id='startChannel[%d]' name='startChannel[%d]' type='text' size='5' maxlength='5' value='%d'></td>
", $i, $i + 1, $i, $i, $nodeCount, $i, $i, $startChannel);

			if (($i % 4) == 0)
				printf("<td class='center' rowspan='4'><select id='nodeType[%d]' name='nodeType[%d]'>" . printNodeTypeOptions($nodeType) . "</select></td>\n", $i, $i);
			else
				printf( "<input type='hidden' name='nodeType[%d]' />", $i);

			printf( "<td class='center'><select id='rgbOrder[%d]' name='rgbOrder[%d]'>" . printRGBColorOptions($rgbOrder) . "</select></td>
	<td class='center'><select id='direction[%d]' name='direction[%d]'>" . printDirectionOptions($direction) . "</select></td>
	<td class='center'><input id='groupCount[%d]' name='groupCount[%d]' type='text' size='3' maxlength='3' value='%d'></td>
	<td class='center'><input id='nullNodes[%d]' name='nullNodes[%d]' type='text' size='3' maxlength='3' value='%d'></td>
</tr>", $i, $i, $i, $i, $i, $i, $groupCount, $i, $i, $nullNodes);
		}
	}
}

?>

<style>
.clear {
	clear: both;
}
.items {
	width: 40%;
	background: rgb#FFF;
	float: right;
	margin: 0, auto;
}
.selectedEntry {
	background: #CCC;
}
.pl_title {
	font-size: larger;
}
h4, h3 {
	padding: 0;
	margin: 0;
}
.tblheader {
	background-color: #CCC;
	text-align: center;
}
tr.rowScheduleDetails {
	border: thin solid;
	border-color: #CCC;
}
tr.rowScheduleDetails td {
	padding: 1px 5px;
}
#tblHardware {
	border: thin;
	border-color: #333;
	border-collapse: collapse;
}
a:active {
	color: none;
}
a:visited {
	color: blue;
}
.time {
	width: 100%;
}
.center {
	text-align: center;
}
</style>

<title><? echo $pageTitle; ?></title>

<script>
		var HardwareEntrySelected = -1;

    $(function() {
		$('#tblHardware').on('mousedown', 'tr', function(event,ui){
					$('#tblHardware tr').removeClass('selectedEntry');
					$('#tblHardware tr td').removeClass('selectedEntry');
					if ($(this).attr('name') == "hrRow")
					{
						HardwareEntrySelected = -1;
						return;
					}

          $(this).addClass('selectedEntry');
					var items = $('#tblHardware tr');
					HardwareEntrySelected  = items.index(this);
					var hwRow = parseInt($(this).attr('id').substring(5));
					if ((hwRow % 4) != 0)
					{

						hwRow = Math.floor(hwRow / 4) * 4;
						$('#hwRow' + hwRow + " td:nth-child(4)").addClass('selectedEntry');
					}
		});
	});
</script>
</head>

<body>
<div id='bodyWrapper'>
	<?php include 'menu.inc'; ?>
	<div style='margin:0 auto;'> <br />
		<fieldset style='padding: 10px; border: 2px solid #000;'>
			<legend>Falcon <? echo $hwModel; ?></legend>
			<div>
				<table>
					<tr><th align='left'>Model:</th><td><? echo $hwModel; ?></td></tr>
					<tr><th align='left'>Firmware:</th><td><? echo $hwFWVer; ?></td></tr>
				</table>
				<br>
				<form id='frmHardware'>
					<input name='command' type='hidden' value='saveHardwareConfig' />
					<input name='model' type='hidden' value='<? echo $hwModel; ?>' />
					<input name='firmware' type='hidden' value='<? echo $hwFWVer; ?>' />
					<table>
						<tr>
							<td width = '70 px'><input id='btnSaveHardwareConfig' class='buttons' type='button' value='Save' onClick='SaveHardwareConfig();'/></td>
						</tr>
					</table>
					<table id='tblHardware'>
<? printFalconConfig(); ?>
					</table>
				</form>
			</div>
		</fieldset>
	</div>
	<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
