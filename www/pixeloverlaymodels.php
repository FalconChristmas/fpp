<?php
require_once("common.php");

?>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

function GetOrientationInput(currentValue) {
	var options = {
		horizontal: "Horizontal",
		vertical:   "Vertical"
		};
	var str = "<select class='orientation'>";

	for (var key in options) {
		str += "<option value='" + key + "'";
		if (currentValue == key)
			str += "selected";

		str += ">" + options[key] + "</option>";
	}

	str += "</select>";

	return str;
}

function GetStartingCornerInput(currentValue) {
	var options = {
		TL: "Top Left",
		TR: "Top Right",
		BL: "Bottom Left",
		BR: "Bottom Right"
		};
	var str = "<select class='corner'>";

	for (var key in options) {
		str += "<option value='" + key + "'";
		if (currentValue == key)
			str += "selected";

		str += ">" + options[key] + "</option>";
	}

	str += "</select>";

	return str;
}

function PopulateChannelMemMapTable(data) {
	$('#channelMemMaps tbody').html("");

	for (var i = 0; i < data.length; i++) {
		$('#channelMemMaps tbody').append("<tr id='row'" + i + " class='fppTableRow'>" +
			"<td><input class='blk' type='text' size='31' maxlength='31' value='" + data[i].BlockName + "'></td>" +
			"<td><input class='start' type='text' size='6' maxlength='6' value='" + data[i].StartChannel + "'></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value='" + data[i].ChannelCount + "'></td>" +
			"<td>" + GetOrientationInput(data[i].Orientation) + "</td>" +
			"<td>" + GetStartingCornerInput(data[i].StartCorner) + "</td>" +
			"<td><input class='strcnt' type='text' size='3' maxlength='3' value='" + data[i].StringCount + "'></td>" +
			"<td><input class='strands' type='text' size='2' maxlength='2' value='" + data[i].StrandsPerString + "'></td>" +
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
			ChannelCount: $this.find("input.cnt").val(),
			Orientation: $this.find("select.orientation").val(),
			StartCorner: $this.find("select.corner").val(),
			StringCount: $this.find("input.strcnt").val(),
			StrandsPerString: $this.find("input.strands").val()
			};

		if ((memmap.BlockName != "") &&
			(parseInt(memmap.StartChannel) > 0) &&
			(parseInt(memmap.ChannelCount) > 0) &&
			(parseInt(memmap.StringCount) > 0) &&
			(parseInt(memmap.StrandsPerString) > 0))
		{
			if (postData != "") {
				postData += ", ";
			}

			postData += JSON.stringify(memmap);
		} else {
			dataError = 1;
			// FIXME, put in some more info here, highlight bad field, etc.
			alert("MemMap '" + memmap.BlockName + "' starting at channel '" + memmap.StartChannel + "' containing '" + memmap.ChannelCount + "' channel(s) is not valid.");
			return;
		}
	});

	if (dataError != 0)
		return;

	postData = "command=setChannelMemMaps&data=[ " + postData + " ]";

	$.post("fppjson.php", postData).success(function(data) {
		$.jGrowl("Pixel Overlay Models saved.");
		PopulateChannelMemMapTable(data);
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Pixel Overlay Models", "Save Failed!");
	});
}

function AddNewMemMap() {
	var currentRows = $("#channelMemMaps > tbody > tr").length

	$('#channelMemMaps tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
			"<td><input class='blk' type='text' size='31' maxlength='31' value=''></td>" +
			"<td><input class='start' type='text' size='6' maxlength='6' value=''></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value=''></td>" +
			"<td>" + GetOrientationInput('') + "</td>" +
			"<td>" + GetStartingCornerInput('') + "</td>" +
			"<td><input class='strcnt' type='text' size='3' maxlength='3' value='1'></td>" +
			"<td><input class='strands' type='text' size='2' maxlength='2' value='1'></td>" +
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

<title><? echo $pageTitle; ?></title>
</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

		<div id="time" class="settings">
			<fieldset>
				<legend>Pixel Overlay Models</legend>
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
								<th title='Name of Model'>Model Name</td>
								<th title='Start Channel'>Start Ch.</td>
								<th title='Channel Count'>Ch. Count</td>
								<th title='String Orientation'>Orientation</td>
								<th title='Starting Corner'>Start Corner</td>
								<th title='Number of Strings'>Strings</td>
								<th title='Number of Strands Per String'>Strands</td>
							</tr>
						</thead>
						<tbody>
						</tbody>
					</table>
				</div>
				<br>
				<font size=-1>
					The Real-Time Pixel Overlay system and Pixel Overlay Models
					allow overlaying user-provided data onto outoing channel
					data via a special interface.
				</font>
			</fieldset>
		</div>

		<?php	include 'common/footer.inc'; ?>
	</div>

<script language="Javascript">
	$('#channelMemMaps').tooltip({
		content: function() {
			return $(this).attr('title');
		}
	});
</script>

</body>
</html>
