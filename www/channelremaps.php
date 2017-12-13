<?php
require_once("common.php");

?>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

function PopulateChannelRemapTable(data) {
	$('#channelRemaps tbody').html("");

	for (var i = 0; i < data.length; i++) {
		$('#channelRemaps tbody').append("<tr id='row'" + i + " class='fppTableRow'>" +
			"<td><input class='src' type='text' size='6' maxlength='6' value='" + data[i].Source + "'></td>" +
			"<td><input class='dst' type='text' size='6' maxlength='6' value='" + data[i].Destination + "'></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value='" + data[i].Count + "'></td>" +
			"</tr>");
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

	$('#channelRemaps tbody tr').each(function() {
		$this = $(this);

		var remap = {
			Source: $this.find("input.src").val(),
			Destination: $this.find("input.dst").val(),
			Count: $this.find("input.cnt").val()
			};

		if ((parseInt(remap.Source) > 0) &&
			(parseInt(remap.Destination) > 0) &&
			(parseInt(remap.Count) > 0))
		{
			if (postData != "") {
				postData += ", ";
			}

			postData += JSON.stringify(remap);
		} else {
			dataError = 1;
			alert("Remap " + remap.Count + " channel(s) from " + remap.Source + " to " + remap.Destination + " is not valid.");
			return;
		}
	});

	if (dataError != 0)
		return;

	postData = "command=setChannelRemaps&data=[ " + postData + " ]";

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
			"<td><input class='src' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='dst' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value=''></td>" +
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
								<th>Source</td>
								<th>Destination</td>
								<th>Count</td>
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
				</font>
			</fieldset>
		</div>

	<?php	include 'common/footer.inc'; ?>
	</div>
</body>
</html>
