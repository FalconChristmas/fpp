<!DOCTYPE html>
<html>
<?php
require_once("common.php");

?>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">
var FBDevices = new Array();
var FBInfo = {};
<?
if ($settings["Platform"] != "MacOS") {
    foreach (scandir("/dev/") as $fileName) {
        if (preg_match("/^fb[0-9]+/", $fileName)) {
            echo "FBDevices['$fileName'] = '$fileName';\n";
            echo "FBInfo['$fileName'] = {};\n";

            $geometry = exec("fbset -i -fb /dev/$fileName | grep geometry", $output, $return_val);
            if ($return_val == 0) {
                $parts = preg_split("/\s+/", preg_replace('/^ *geometry */', '', $geometry));
                if (count($parts) > 3) {
                    echo "FBInfo['$fileName'].Width = " . $parts[0] . ";\n";
                    echo "FBInfo['$fileName'].Height = " . $parts[1] . ";\n";
                }
            }
        }
    }
}

if (($settings['Platform'] == "Linux") && (file_exists('/usr/include/X11/Xlib.h'))) {
?>
FBDevices['x11'] = 'x11';
FBInfo['x11'] = {};
FBInfo['x11'].Width = 640;
FBInfo['x11'].Height = 480;
<?
}

?>

function GetOrientationInput(currentValue, attr) {

	var options = {
		horizontal: "Horizontal",
		vertical:   "Vertical"
		};
    var str = "";
    const is_custom = (currentValue || "").toLowerCase().startsWith("custom");
    if (is_custom) {
        str = "<b>" + currentValue + "</b>"; //allow descriptive text following
    }
	str += "<select class='orientation'";
    str += attr;
    if (is_custom) {
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

function DeviceChanged(item) {
    var type = $(item).parent().parent().find('span.type').html();

    if (type == 'FB') {
        var device = $(item).val();

        $(item).parent().parent().find('input.width').val(FBInfo[device].Width);
        $(item).parent().parent().find('input.height').val(FBInfo[device].Height);
    }
}

function WidthOrHeightModified(item) {
    var width = parseInt($(item).parent().parent().find('input.width').val());
    var height = parseInt($(item).parent().parent().find('input.height').val());
    var channels = width * height * 3;
    $(item).parent().parent().find('td.channels').html(channels);
}

function PopulateChannelMemMapTable(data) {
	$('#channelMemMaps tbody').html("");
    $('#channelMemMapsAutoCreate tbody').html("");
    if (data == null) {
        return;
    }

    for (var i = 0; i < data.length; i++) {
        var model = data[i];
        var ChannelCountPerNode = model.ChannelCountPerNode;
        if (ChannelCountPerNode == undefined) {
            ChannelCountPerNode = 3;
        }
		postr = "<tr id='row'" + i + " class='fppTableRow'>" +
			"<td class='center' valign='middle'>";

        var attr = " ";
        if (model.autoCreated) {
            postr += "";
            attr = " disabled";
        } else {
            postr += "<div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div>";
        }

        postr += "</td><td><input class='blk' type='text' size='31' maxlength='31' value='" + model.Name + "'" + attr + "></td>";

        switch (model.Type) {
            case "Channel":
                postr += "<td><span class='hidden type'>" + model.Type + "</span>" + model.Type + "</td>" +
                    "<td><input class='start' type='text' size='6' maxlength='6' value='" + model.StartChannel + "'" + attr + "></td>" +
                    "<td><input class='cnt' type='text' size='6' maxlength='6' value='" + model.ChannelCount + "'" + attr + "></td>" +
                    "<td><input class='cpn' type='number' min='1' max='4' value='" + ChannelCountPerNode + "'" + attr + "></td>" +
                    "<td style=\"white-space: nowrap;\">" + GetOrientationInput(model.Orientation + orientationDetails(model), attr) + "</td>";
                if (model.Orientation != "custom") {
                    postr += "<td>" + GetStartingCornerInput(model.StartCorner, attr) + "</td>" +
                    "<td><input class='strcnt' type='text' size='3' maxlength='3' value='" + model.StringCount + "'" + attr + "></td>" +
                    "<td><input class='strands' type='text' size='2' maxlength='2' value='" + model.StrandsPerString + "'" + attr + "></td><td>";
                } else {
                    postr += "<td><input class='corner' type='hidden' value='" + model.StartCorner + "'><input class='data' type='hidden' value='" + model.data + "'></td>" +
                        "<td><input class='strcnt' type='hidden' value='" + model.StringCount + "'></td>" +
                        "<td><input class='strands' type='hidden' value='" + model.StrandsPerString + "'></td><td>";
                }
                break;
            case "FB":
                //alert('JSON: ' + JSON.stringify(model));
                postr += "<td><span class='hidden type'>FB</span>FrameBuffer</td>" + 
                    "<td>" + CreateSelect(FBDevices, model.Device, '', '-- Port --', 'device', 'DeviceChanged(this);') + "</td>" +
                    "<td class='channels'>" + (model.Width * model.Height * 3) + "</td><td>3</td><td>Horizontal</td><td>Top Left</td>" +
                    "<td colspan='2'><input class='width' type='number' min='0' max='4096' step='8' value='" + model.Width + "'" + attr + " onChange='WidthOrHeightModified(this);'> <b>X</b>" +
                        "<input class='height' type='number' min='0' max='2160' step='8' value='" + model.Height + "'" + attr + " onChange='WidthOrHeightModified(this);'></td>" +
                    "<td>";
                break;
        }

        if (model.effectRunning) {
            postr += model.effectName;
        }
        if (model.autoCreated) {
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

//give a little more detail about custom models:
function orientationDetails(data)
{
    if (data.Orientation != "custom" || !data.hasOwnProperty("data")) return "";
//for now, just show grid size
    const rows = data.data.split(";"), cols = rows[0].split(",");
    return "&nbsp;" + cols.length + "&nbsp;x&nbsp;" + rows.length;
}

function SetChannelMemMaps() {
    var postData = "";
	var dataError = 0;
	var data = {};
	var models = [];

	$('#channelMemMaps tbody tr').each(function() {
		$this = $(this);

		var model = {
            Name: $this.find("input.blk").val(),
            Type: $this.find("span.type").html(),
			};

        if ((model.Name != "") && (model.Type != "")) {
            switch (model.Type) {
                case "Channel":
                    model.StartChannel = parseInt($this.find("input.start").val());
                    model.ChannelCount = parseInt($this.find("input.cnt").val());
                    model.Orientation = $this.find("select.orientation").val();
                    model.StartCorner = $this.find("select.corner").val();
                    model.StringCount = parseInt($this.find("input.strcnt").val());
                    model.StrandsPerString = parseInt($this.find("input.strands").val());
                    model.ChannelCountPerNode = parseInt($this.find("input.cpn").val());

                    if ((model.StartChannel > 0) &&
                        (model.ChannelCount > 0)) {
                        if (model.Orientation == "custom") {
                            model.data = $this.find("input.data").val();
                            model.StartCorner = $this.find("input.corner").val();
                            models.push(model);
                        } else if ((model.StringCount > 0) &&
                                   (model.StrandsPerString > 0)) {
                            models.push(model);
                        } else {
                            dataError = 1;
                            // FIXME, put in some more info here, highlight bad field, etc.
                            alert("MemMap '" + model.BlockName + "' starting at channel '" + model.StartChannel + "' containing '" + model.ChannelCount + "' channel(s) is not valid.");
                            return;
                        }
                    }
                    break;
                case "FB":
                    model.Width = parseInt($this.find("input.width").val());
                    model.Height = parseInt($this.find("input.height").val());
                    model.Device = $this.find("select.device").val();

                    // -1 indicates use default value
                    model.Width = model.Width > 0 ? model.Width : -1;
                    model.Height = model.Height > 0 ? model.Height : -1;

                    models.push(model);
                    break;
            }
        } else {
            dataError = 1;
            // FIXME, put in some more info here, highlight bad field, etc.
            alert("MemMap '" + model.BlockName + "' starting at channel '" + model.StartChannel + "' containing '" + model.ChannelCount + "' channel(s) is not valid.");
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

function AddNewChannelModel() {
    var currentRows = $("#channelMemMaps > tbody > tr").length;
    $('#channelMemMaps tbody').append(
        "<tr id='row'" + currentRows + " class='fppTableRow'>" +
            "<td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>"  +
            "<td><input class='blk' type='text' size='31' maxlength='31' value=''></td>" +
            "<td><span class='hidden type'>Channel</span>Channel</td>" + 
            "<td><input class='start' type='text' size='7' maxlength='7' value=''></td>" +
            "<td><input class='cnt' type='text' size='6' maxlength='6' value=''></td>" +
            "<td><input class='cpn' type='number' min='1' max='4' value='3'></td>" +
            "<td>" + GetOrientationInput('') + "</td>" +
            "<td>" + GetStartingCornerInput('') + "</td>" +
            "<td><input class='strcnt' type='text' size='3' maxlength='3' value='1'></td>" +
            "<td><input class='strands' type='text' size='2' maxlength='2' value='1'></td>" +
            "<td></td>" +
            "</tr>");
    $(this).fppDialog("close");
}

function AddNewFBModel() {
    var currentRows = $("#channelMemMaps > tbody > tr").length;
    var device = 'fb0';
    $('#channelMemMaps tbody').append(
        "<tr id='row'" + currentRows + " class='fppTableRow'>" +
            "<td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>"  +
            "<td><input class='blk' type='text' size='31' maxlength='31' value='fb0'></td>" +
            "<td><span class='hidden type'>FB</span>FrameBuffer</td>" + 
            "<td>" + CreateSelect(FBDevices, device, '', '-- Port --', 'device', 'DeviceChanged(this);') + "</td>" +
            "<td class='channels'>" + (FBInfo[device].Width * FBInfo[device].Height * 3) + "</td><td>3</td><td>Horizontal</td><td>Top Left</td>" +
            "<td colspan='2'><input class='width' type='number' min='0' max='4096' step='8' value='" + FBInfo[device].Width + "' onChange='WidthOrHeightModified(this);'> <b>X</b>" +
            "<input class='height' type='number' min='0' max='2160' step='8' value='" + FBInfo[device].Height + "' onChange='WidthOrHeightModified(this);'></td>" +
            "<td></td>" +
            "</tr>");
    $(this).fppDialog("close");
}

function AddNewModel() {
    if (settings['Platform'] == 'MacOS') {
        AddNewChannelModel();
        return;
    }

    $('#addDialog').fppDialog({
        autoOpen: true,
        buttons: {
            Channel: function() {
                AddNewChannelModel();
            },

            FrameBuffer: function() {
                AddNewFBModel();
            }
        }
    });
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
								<button type=button value='Add' onClick='AddNewModel();' class='buttons btn-outline-success'><i class="fas fa-plus"></i> Add</button>
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
                                    <th title='Type'>Type</th>
                                    <th title='Start Channel'>Start Ch.</th>
                                    <th title='Channel Count'>Ch. Count</th>
                                    <th title='Chan Per Node'>Ch./Node</th>
                                    <th title='String Orientation'>Orientation</th>
                                    <th title='Starting Corner'>Start Corner</th>
                                    <th title='Number of Strings or Width of FB/X11/Sub-Model'>Strings</th>
                                    <th title='Number of Strands Per String or Height of FB/X11/Sub-Model'>Strands</th>
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
                                    <th title='Type'>Type</th>
                                    <th title='Start Channel'>Start Ch.</th>
                                    <th title='Channel Count'>Ch. Count</th>
                                    <th title='Chan Per Node'>Ch./Node</th>
                                    <th title='String Orientation'>Orientation</th>
                                    <th title='Starting Corner'>Start Corner</th>
                                    <th title='Number of Strings or Width of FB/X11/Sub-Model'>Strings</th>
                                    <th title='Number of Strands Per String or Height of FB/X11/Sub-Model'>Strands</th>
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

<div id='addDialog' style='display: none;'>Choose the type of model to add.</div>
</body>
</html>
