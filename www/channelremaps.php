<?php
require_once("common.php");

?>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

function PopulateChannelRemapTable(data) {
	$('#channelRemaps tbody').html("");

	for (var i = 0; i < data.remaps.length; i++) {
		var html =
			"<tr id='row'" + i + " class='fppTableRow'>" +
			"<td><input class='active' type='checkbox'";

		if (data.remaps[i].active)
			html += " checked";

		html += "></td>" +
			"<td><input class='description' type='text' size='32' maxlength='64' value='" + data.remaps[i].description + "'></td>" +
			"<td><input class='source' type='text' size='6' maxlength='6' value='" + data.remaps[i].source + "'></td>" +
			"<td><input class='destination' type='text' size='6' maxlength='6' value='" + data.remaps[i].destination + "'></td>" +
			"<td><input class='count' type='text' size='6' maxlength='6' value='" + data.remaps[i].count + "'></td>" +
			"<td><input class='loops' type='text' size='6' maxlength='6' value='" + data.remaps[i].loops + "'></td>" +
			"</tr>";

		$('#channelRemaps tbody').append(html);
	}
}

function GetChannelRemaps() {
	$.getJSON("fppjson.php?command=getChannelRemaps", function(data) {
		PopulateChannelRemapTable(data);
	});
}

function SetChannelRemaps() {
	var postData = "";
	var dataError = 0;
	var data = {};
	var remaps = [];

	$('#channelRemaps tbody tr').each(function() {
		$this = $(this);

		var remap = {
			active: $this.find("input.active").is(':checked') ? 1 : 0,
			description: $this.find("input.description").val(),
			source: parseInt($this.find("input.source").val()),
			destination: parseInt($this.find("input.destination").val()),
			count: parseInt($this.find("input.count").val()),
			loops: parseInt($this.find("input.loops").val())
			};

		if ((remap.source > 0) &&
			(remap.destination > 0) &&
			(remap.count > 0) &&
			(remap.loops > 0))
		{
			remaps.push(remap);
		} else {
			dataError = 1;
			alert("Remap '" + remap.count + "' channel(s) from '" + remap.source + "' to '" + remap.destination + "' is not valid.");
			return;
		}
	});

	if (dataError != 0)
		return;

	data.remaps = remaps;
	postData = "command=setChannelRemaps&data={ " + JSON.stringify(data) + " }";

	$.post("fppjson.php", postData).success(function(data) {
		$.jGrowl("Channel Remap Table saved");
		PopulateChannelRemapTable(data);
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Channel Remap Table", "Save Failed");
	});
}

function AddNewRemap() {
	var currentRows = $("#channelRemaps > tbody > tr").length

	$('#channelRemaps tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
			"<td><input class='active' type='checkbox' checked>" +
			"<td><input class='description' type='text' size='32' maxlength='64' value=''></td>" +
			"<td><input class='source' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='destination' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='count' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='loops' type='text' size='6' maxlength='6' value='1'></td>" +
			"</tr>");
}

var tableInfo = {
	tableName: "channelRemaps",
	selected:  -1,
	enableButtons: [ "btnDelete" ],
	disableButtons: []
	};

function DeleteSelectedRemap() {
	if (tableInfo.selected >= 0) {
		$('#channelRemaps tbody tr:nth-child(' + (tableInfo.selected+1) + ')').remove();
		tableInfo.selected = -1;
		SetButtonState("#btnDelete", "disable");
	}
}

$(document).ready(function(){
	SetupSelectableTableRow(tableInfo);
	GetChannelRemaps();
});

$(document).tooltip();

</script>

<title><? echo $pageTitle; ?></title>
</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

		<div id="time" class="settings">
			<fieldset>
				<legend>Remap Output Channels</legend>
				<table>
					<tr>
						<td width='70px'><input type=button value='Save' onClick='SetChannelRemaps();' class='buttons'></td>
						<td width='70px'><input type=button value='Add' onClick='AddNewRemap();' class='buttons'></td>
						<td width='40px'>&nbsp;</td>
						<td width='70px'><input type=button value='Delete' onClick='DeleteSelectedRemap();' id='btnDelete' class='disableButtons'></td>
					</tr>
				</table>
				<div class="fppTableWrapper">
					<table id="channelRemaps">
						<thead>
							<tr class="fppTableHeader">
								<th>Active</td>
								<th>Description</td>
								<th>Source</td>
								<th>Destination</td>
								<th>Channel Count</td>
								<th>Loops</td>
							</tr>
						</thead>
						<tbody>
						</tbody>
					</table>
				</div>
				<br>
				<font size=-1>
					You may remap one or more blocks of channels to different output channel
					numbers by specifying the source and destination
					channel numbers along with the count of the number of channels to remap.
					The Loops count allows you to copy the same set of source channels multiple times to the destination, similar to grouping on a pixel controller output.
				</font>
			</fieldset>
		</div>

	<?php	include 'common/footer.inc'; ?>
	</div>
</body>
</html>
