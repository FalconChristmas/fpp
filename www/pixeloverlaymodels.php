<!DOCTYPE html>
<html>
<?php
require_once("common.php");

?>
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
			"<td><input class='blk' type='text' size='31' maxlength='31' value='" + data[i].Name + "'></td>" +
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
	$.get("/api/models", function(data) {
		PopulateChannelMemMapTable(data);
	}).fail(function() {
		DialogError("Load Pixel Overlay Models", "Load Failed, is fppd running?");
	});
}

function SetChannelMemMaps() {
    var postData = "";
	var dataError = 0;
	var data = {};
	var models = [];

	$('#channelMemMaps tbody tr').each(function() {
		$this = $(this);

		var memmap = {
			Name: $this.find("input.blk").val(),
			StartChannel: parseInt($this.find("input.start").val()),
			ChannelCount: parseInt($this.find("input.cnt").val()),
			Orientation: $this.find("select.orientation").val(),
			StartCorner: $this.find("select.corner").val(),
			StringCount: parseInt($this.find("input.strcnt").val()),
			StrandsPerString: parseInt($this.find("input.strands").val())
			};

		if ((memmap.Name != "") &&
			(memmap.StartChannel > 0) &&
			(memmap.ChannelCount > 0) &&
			(memmap.StringCount > 0) &&
			(memmap.StrandsPerString > 0))
		{
			models.push(memmap);
		} else {
			dataError = 1;
			// FIXME, put in some more info here, highlight bad field, etc.
			alert("MemMap '" + memmap.BlockName + "' starting at channel '" + memmap.StartChannel + "' containing '" + memmap.ChannelCount + "' channel(s) is not valid.");
			return;
		}
	});

	if (dataError != 0)
		return;

	data.models = models;
	postData = JSON.stringify(data, null, 2);

	$.post("/api/models", postData).done(function(data) {
		$.jGrowl("Pixel Overlay Models saved.");
		SetRestartFlag(2);
	}).fail(function() {
		DialogError("Save Pixel Overlay Models", "Save Failed, is fppd running?");
	});
}

function AddNewMemMap() {
	var currentRows = $("#channelMemMaps > tbody > tr").length;

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
	disableButtons: [],
    sortable: 1
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
				<div class='fppTableWrapper fppTableWrapperAsTable'>
                    <div class='fppTableContents'>
                        <table id="channelMemMaps">
                            <thead>
                                <tr>
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
				</div>
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
