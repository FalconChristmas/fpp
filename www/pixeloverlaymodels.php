<!DOCTYPE html>
<html>
<?php
require_once("common.php");

?>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

function GetOrientationInput(currentValue, attr) {

	var options = {
		horizontal: "Horizontal",
		vertical:   "Vertical"
		};
    var str = "";
    if (currentValue == "custom") {
        str = "<b>Custom</b>";
    }
	str += "<select class='orientation'";
    str += attr;
    if (currentValue == "custom") {
        str += " style='visibility: hidden;'><option value='custom' selected>Custom</option";
    }
    str += ">";
	for (var key in options) {
		str += "<option value='" + key + "'";
		if (currentValue == key)
			str += "selected";

		str += ">" + options[key] + "</option>";
	}
    
	str += "</select>";

	return str;
}

function GetStartingCornerInput(currentValue, attr) {
	var options = {
		TL: "Top Left",
		TR: "Top Right",
		BL: "Bottom Left",
		BR: "Bottom Right"
		};
	var str = "<select class='corner'" + attr + ">";

	for (var key in options) {
		str += "<option value='" + key + "'";
		if (currentValue == key)
			str += "selected";

		str += ">" + options[key] + "</option>";
	}

	str += "</select>";

	return str;
}
function PopulatePixelOverlaySettings(data) {
    if (data != null) {
        $("#AutoCreatePixelOverlays").prop('checked', data["autoCreate"]);
    }
}
function PopulateChannelMemMapTable(data) {
	$('#channelMemMaps tbody').html("");
    $('#channelMemMapsAutoCreate tbody').html("");
    if (data == null) {
        return;
    }

	for (var i = 0; i < data.length; i++) {
        var ChannelCountPerNode = data[i].ChannelCountPerNode;
        if (ChannelCountPerNode == undefined) {
            ChannelCountPerNode = 3;
        }
		postr = "<tr id='row'" + i + " class='fppTableRow'>" +
			"<td class='center' valign='middle'>";

        var attr = " ";
        if (data[i].autoCreated) {
            postr += "";
            attr = " disabled";
        } else {
            postr += "<div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div>";
        }

        postr += "</td><td><input class='blk' type='text' size='31' maxlength='31' value='" + data[i].Name + "'" + attr + "></td>" +
			"<td><input class='start' type='text' size='6' maxlength='6' value='" + data[i].StartChannel + "'" + attr + "></td>" +
            "<td><input class='cnt' type='text' size='6' maxlength='6' value='" + data[i].ChannelCount + "'" + attr + "></td>" +
            "<td><input class='cpn' type='number' min='1' max='4' value='" + ChannelCountPerNode + "'" + attr + "></td>" +
            "<td>" + GetOrientationInput(data[i].Orientation, attr) + "</td>";
        if (data[i].Orientation != "custom") {
            postr += "<td>" + GetStartingCornerInput(data[i].StartCorner, attr) + "</td>" +
		    "<td><input class='strcnt' type='text' size='3' maxlength='3' value='" + data[i].StringCount + "'" + attr + "></td>" +
            "<td><input class='strands' type='text' size='2' maxlength='2' value='" + data[i].StrandsPerString + "'" + attr + "></td><td>";
        } else {
            postr += "<td><input class='corner' type='hidden' value='" + data[i].StartCorner + "'><input class='data' type='hidden' value='" + data[i].data + "'></td>" +
                "<td><input class='strcnt' type='hidden' value='" + data[i].StringCount + "'></td>" +
                "<td><input class='strands' type='hidden' value='" + data[i].StrandsPerString + "'></td><td>";
        }
        if (data[i].effectRunning) {
            postr += data[i].effectName;
        }
        if (data[i].autoCreated) {
            $('#channelMemMapsAutoCreate tbody').append(postr + "</td></tr>");
        } else {
            $('#channelMemMaps tbody').append(postr + "</td></tr>");
        }
	}
}

function GetChannelMemMaps() {
	$.get("api/overlays/models", function(data) {
		PopulateChannelMemMapTable(data);
	}).fail(function() {
		DialogError("Load Pixel Overlay Models", "Load Failed, is fppd running?");
	});
    $.get("api/overlays/settings", function(data) {
        PopulatePixelOverlaySettings(data);
    }).fail(function() {
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
			StrandsPerString: parseInt($this.find("input.strands").val()),
            ChannelCountPerNode: parseInt($this.find("input.cpn").val())
			};

        if ((memmap.Name != "") &&
            (memmap.StartChannel > 0) &&
            (memmap.ChannelCount > 0)) {
            if (memmap.Orientation == "custom") {
                memmap.data = $this.find("input.data").val();
                memmap.StartCorner = $this.find("input.corner").val();
                models.push(memmap);
            } else if ((memmap.StringCount > 0) &&
                       (memmap.StrandsPerString > 0)) {
                models.push(memmap);
            } else {
                dataError = 1;
                // FIXME, put in some more info here, highlight bad field, etc.
			    alert("MemMap '" + memmap.BlockName + "' starting at channel '" + memmap.StartChannel + "' containing '" + memmap.ChannelCount + "' channel(s) is not valid.");
			    return;
            }
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
    data.autoCreate = $("#AutoCreatePixelOverlays").is(':checked');
	postData = JSON.stringify(data, null, 2);

	$.post("api/models", postData).done(function(data) {
		$.jGrowl("Pixel Overlay Models saved.",{themeState:'success'});
		SetRestartFlag(2);
	}).fail(function() {
		DialogError("Save Pixel Overlay Models", "Save Failed, is fppd running?");
	});
}

function AddNewMemMap() {
	var currentRows = $("#channelMemMaps > tbody > tr").length;

	$('#channelMemMaps tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
			"<td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>"  +
			"<td><input class='blk' type='text' size='31' maxlength='31' value=''></td>" +
			"<td><input class='start' type='text' size='7' maxlength='7' value=''></td>" +
			"<td><input class='cnt' type='text' size='6' maxlength='6' value=''></td>" +
            "<td><input class='cpn' type='number' min='1' max='4' value='3'></td>" +
			"<td>" + GetOrientationInput('') + "</td>" +
			"<td>" + GetStartingCornerInput('') + "</td>" +
			"<td><input class='strcnt' type='text' size='3' maxlength='3' value='1'></td>" +
			"<td><input class='strands' type='text' size='2' maxlength='2' value='1'></td>" +
            "<td></td>" +
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
		//$('#channelMemMaps tbody tr:nth-child(' + (tableInfo.selected+1) + ')').remove();
		$('#channelMemMaps .selectedEntry').remove();
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
		<?php 
		$activeParentMenuItem = 'input-output';
		include 'menu.inc'; ?>
  <div class="mainContainer">
	<h1 class="title">Pixel Overlay Models</h1>
	<div class="pageContent">
		

		<div id="time" class="settings">

				<div class="row tablePageHeader">
					<div class="col-md">
						<h2>Pixel Overlay Models</h2>
					</div>
					<div class="col-md-auto ml-lg-auto">
						<div class="form-actions">
			
								<input type=button value='Delete' onClick='DeleteSelectedMemMap();' data-btn-enabled-class="btn-outline-danger" id='btnDelete' class='disableButtons'>
								<button type=button value='Add' onClick='AddNewMemMap();' class='buttons btn-outline-success'><i class="fas fa-plus"></i> Add</button>
								<input type=button value='Save' onClick='SetChannelMemMaps();' class='buttons btn-success ml-1'>

						</div>
					</div>
				</div>
                <div>
                Create Overlays Automatically From Outputs: <input id="AutoCreatePixelOverlays" type="checkbox" checked/>
                </div>
                <hr>
				<div class='fppTableWrapper fppTableWrapperAsTable'>
                    <div class='fppTableContents' role="region" aria-labelledby="channelMemMaps" tabindex="0">
                        <table id="channelMemMaps" class="fppSelectableRowTable">
                            <thead>
                                <tr>
									<th class="tblChannelMemMapsHeadGrip"></th>
                                    <th title='Name of Model'>Model Name</th>
                                    <th title='Start Channel'>Start Ch.</th>
                                    <th title='Channel Count'>Ch. Count</th>
                                    <th title='Chan Per Node'>Ch./Node</th>
                                    <th title='String Orientation'>Orientation</th>
                                    <th title='Starting Corner'>Start Corner</th>
                                    <th title='Number of Strings'>Strings</th>
                                    <th title='Number of Strands Per String'>Strands</th>
                                    <th title='Running Effect'>Running Effect</th>
                                </tr>
                            </thead>
                            <tbody>
                            </tbody>
                        </table>
                    </div>
				</div>

                <div class="row tableHeader">
                    <div class="col-md">
                        <h2>Auto Created Pixel Overlay Models</h2>
                    </div>
                </div>
				<div class='fppTableWrapper fppTableWrapperAsTable'>
                    <div class='fppTableContents' role="region" aria-labelledby="channelMemMapsAutoCreate" tabindex="0">
                        <table id="channelMemMapsAutoCreate" class="fppSelectableRowTable">
                            <thead>
                                <tr>
									<th class="tblChannelMemMapsHeadGrip"></th>
                                    <th title='Name of Model'>Model Name</th>
                                    <th title='Start Channel'>Start Ch.</th>
                                    <th title='Channel Count'>Ch. Count</th>
                                    <th title='Chan Per Node'>Ch./Node</th>
                                    <th title='String Orientation'>Orientation</th>
                                    <th title='Starting Corner'>Start Corner</th>
                                    <th title='Number of Strings'>Strings</th>
                                    <th title='Number of Strands Per String'>Strands</th>
                                    <th title='Running Effect'>Running Effect</th>
                                </tr>
                            </thead>
                            <tbody>
                            </tbody>
                        </table>
                    </div>
				</div>
	</div>
</div>

		<?php	include 'common/footer.inc'; ?>
	</div>

<script language="Javascript">
	$('#channelMemMaps').tooltip({
		content: function() {
			return $(this).attr('title');
		}
	});

	$(function() {
		$('#channelMemMaps').on('mousedown', 'tr', function(event,ui) {
			HandleTableRowMouseClick(event, $(this));

			console.log(event.target.nodeName);

			if ($('#channelMemMaps > tr.selectedEntry').length > 0) {
				EnableButtonClass('btnDelete');
			} else {
				DisableButtonClass('btnDelete');
			}
		});
	});


</script>

</body>
</html>
