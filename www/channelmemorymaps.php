<?php
require_once("common.php");

?>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

function PopulateChannelMemMapTable(data) {
	$('#channelMemMaps tbody').html("");

	for (var i = 0; i < data.length; i++) {
		$('#channelMemMaps tbody').append("<tr id='row'" + i + " class='fppTableRow'>" +
			"<td><input class='blk' type='text' size='31' maxlength='31' value='" + data[i].BlockName + "'></td>" +
			"<td><input class='start' type='text' size='6' maxlength='6' value='" + data[i].StartChannel + "'></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value='" + data[i].ChannelCount + "'></td>" +
			"</tr>");
	}
}

function GetChannelMemMaps() {
	$.getJSON("fppjson.php?command=getChannelMemMaps", function(data) {
		PopulateChannelMemMapTable(data);
	});
}

function SetChannelMemMaps() {
	var postData = "";
	var dataError = 0;

	$('#channelMemMaps tbody tr').each(function() {
		$this = $(this);

		var memmap = {
			BlockName: $this.find("input.blk").val(),
			StartChannel: $this.find("input.start").val(),
			ChannelCount: $this.find("input.cnt").val()
			};

		if ((memmap.BlockName != "") &&
			(parseInt(memmap.StartChannel) > 0) &&
			(parseInt(memmap.ChannelCount) > 0))
		{
			if (postData != "") {
				postData += ", ";
			}

			postData += JSON.stringify(memmap);
		} else {
			dataError = 1;
			alert("MemMap '" + memmap.BlockName + "' starting at channel '" + memmap.StartChannel + "' containing '" + memmap.ChannelCount + "' channel(s) is not valid.");
			return;
		}
	});

	if (dataError != 0)
		return;

	postData = "command=setChannelMemMaps&data=[ " + postData + " ]";

	$.post("fppjson.php", postData).success(function(data) {
		PopulateChannelMemMapTable(data);
	}).fail(function() {
		alert("something failed");
	});
}

function AddNewMemMap() {
	var currentRows = $("#channelMemMaps > tbody > tr").length

	$('#channelMemMaps tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
			"<td><input class='blk' type='text' size='31' maxlength='31' value=''></td>" +
			"<td><input class='start' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value=''></td>" +
			"</tr>");
}

var tableInfo = {
	tableName: "channelMemMaps",
	selected:  -1,
	enableButtons: [ "btnDelete" ],
	disableButtons: []
	};

function DeleteSelectedMemMap() {
	if (tableInfo.selected >= 0) {
		$('#channelMemMaps tbody tr:nth-child(' + (tableInfo.selected+1) + ')').remove();
		tableInfo.selected = -1;
		SetButtonState("#btnDelete", "disable");
	}
}

$(document).ready(function(){
	SetupSelectableTableRow(tableInfo);
	GetChannelMemMaps();
});

$(document).tooltip();

</script>

<title>Falcon PI Player - FPP</title>
</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

		<div id="time" class="settings">
			<fieldset>
				<legend>Memory Mapped Channel Blocks</legend>
				<table>
					<tr>
						<td width='70px'><input type=button value='Save' onClick='SetChannelMemMaps();' class='buttons'></td>
						<td width='70px'><input type=button value='Add' onClick='AddNewMemMap();' class='buttons'></td>
						<td width='40px'>&nbsp;</td>
						<td width='70px'><input type=button value='Delete' onClick='DeleteSelectedMemMap();' id='btnDelete' class='disableButtons'></td>
					</tr>
				</table>
				<div class="fppTableWrapper">
					<table id="channelMemMaps">
						<thead>
							<tr class="fppTableHeader">
								<th>Block Name</td>
								<th>Start</td>
								<th>Count</td>
							</tr>
						</thead>
						<tbody>
						</tbody>
					</table>
				</div>
				<br>
				<font size=-1>
					Memory Mapped Channel Blocks allow overlaying
					user-provided data onto outoing channel data via a
					special memory mapped file interface.
				</font>
			</fieldset>
		</div>
	</div>

	<?php	include 'common/footer.inc'; ?>
</body>
</html>
