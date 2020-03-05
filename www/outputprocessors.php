<!DOCTYPE html>
<html>
<?php
require_once("common.php");
?>
<head>
<?php include 'common/menuHead.inc'; ?>
<script language="Javascript">

function outputOption(val, def) {
    var html = "<option value='" + val + "'";
    if (val == def) {
        html += " selected"
    }
    html += ">" + val + "</option>";
    
    return html;
}

function PopulateOutputProcessorTable(data) {
	$('#outputProcessors tbody').html("");

	for (var i = 0; i < data.outputProcessors.length; i++) {
        var output = data.outputProcessors[i];
        var type = output.type;
        
		var html =
			"<tr id='row'" + i + " class='fppTableRow'>" +
            "<td>" + (i+1) + "</td>" +
			"<td><input class='active' type='checkbox'";

		if (output.active)
			html += " checked";

		html += "></td>"
            + "<td>" + type + "</td>"
            + "<td><input class='description' type='text' size='32' maxlength='64' value='" + output.description + "'></td><td>";
        
        if (type == "Remap") {
            html += "Source Channel: <input class='source' type=text  size='7' maxlength='7' value='" + output.source + "'/>&nbsp;"
                      + "Destination: <input class='destination' type=text size='7' maxlength='7' value='" + output.destination + "'/>&nbsp;"
                      + "Count: <input class='count' type=text size='7' maxlength='7' value='" + output.count + "' />&nbsp;"
                      + "Loops: <input class='loops' type=text size='7' maxlength='7' value='" + output.loops + "'/>&nbsp;"
                      + "Reverse: <select class='reverse'>";
			html += "<option value='0' ";
			if (output.reverse == 0) html += "selected";
			html += ">None</option>";
			html += "<option value='1' ";
			if (output.reverse == 1) html += "selected";
			html += ">By Channel</option>";
			html += "<option value='2' ";
			if (output.reverse == 2) html += "selected";
			html += ">RGB Pixels</option>";
			html += "<option value='3' ";
			if (output.reverse == 3) html += "selected";
			html += ">RGBW Pixels</option>";
			html += "</select>";
        } else if (type == "Brightness") {
            html += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='" + output.start + "'/>&nbsp;"
                + "Channel Count: <input class='count' type=text size='7' maxlength='7' value='" + output.count + "'/>&nbsp;"
                + "Brightness: <input class='brightness' type=number value='" + output.brightness + "' min='0' max='100'/>"
                + "Gamma: <input class='gamma' type=number step='0.1' value='" + output.gamma + "' min='0.1' max='5.0'/>";
        } else if (type == "Set Value") {
            html += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='" + output.start + "'/>&nbsp;"
                + "Channel Count: <input class='count' type=text size='7' maxlength='7' value='" + output.count + "'/>&nbsp;"
                + "Value: <input class='value' type=number value='" + output.value + "' min='0' max='255'/>";
        } else if (type == "Hold Value") {
            html += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='" + output.start + "'/>&nbsp;"
                + "Channel Count: <input class='count' type=text size='7' maxlength='7' value='" + output.count + "'/>";
        } else if (type == "Reorder Colors") {
            html += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='" + output.start + "'/>&nbsp;"
                + "Nodes: <input class='count' type=text size='7' maxlength='7' value='" + output.count + "'/>&nbsp;"
                + "Color Order: <select class='colorOrder'>"
                + outputOption("132", output.colorOrder)
                + outputOption("213", output.colorOrder)
                + outputOption("231", output.colorOrder)
                + outputOption("312", output.colorOrder)
                + outputOption("321", output.colorOrder)
                + "</select>";
        } else {
            html += "unknown type " + type;
        }
        
		html += "</td></tr>";

		$('#outputProcessors tbody').append(html);
	}
}

function GetOutputProcessors() {
	$.getJSON("fppjson.php?command=getOutputProcessors", function(data) {
		PopulateOutputProcessorTable(data);
	});
}

function SetOutputProcessors() {
	var postData = "";
	var dataError = 0;
	var data = {};
	var processors = [];
    var rowNumber = 1;

	$('#outputProcessors tbody tr').each(function() {
		$this = $(this);
		// Type
		var type = $this.find("td:nth-child(3)").html();

		// User has not selected a type yet
		if (type.indexOf("<select") >= 0) {
			DialogError("Output Processors",
				"Output Processor type must be selected on row " + rowNumber);
			dataError = 1;
			return;
		}
        if (type == "Remap") {
            var remap = {
                type: "Remap",
                active: $this.find("input.active").is(':checked') ? 1 : 0,
                description: $this.find("input.description").val(),
                source: parseInt($this.find("input.source").val()),
                destination: parseInt($this.find("input.destination").val()),
                count: parseInt($this.find("input.count").val()),
                loops: parseInt($this.find("input.loops").val()),
                reverse: parseInt($this.find("select.reverse").val())
			};
            if ((remap.source > 0) &&
                (remap.destination > 0) &&
                (remap.count > 0) &&
                (remap.loops > 0)) {
                processors.push(remap);
            } else {
                dataError = 1;
                alert("Remap '" + remap.count + "' channel(s) from '" + remap.source + "' to '" + remap.destination + "' is not valid.");
                return;
            }
        } else if (type == "Brightness") {
            var b = {
                type: "Brightness",
                active: $this.find("input.active").is(':checked') ? 1 : 0,
                description: $this.find("input.description").val(),
                start: parseInt($this.find("input.start").val()),
                count: parseInt($this.find("input.count").val()),
                brightness: parseInt($this.find("input.brightness").val()),
                gamma: parseFloat($this.find("input.gamma").val())
			};
            if ((b.start > 0) &&
                (b.count > 0)) {
                processors.push(b);
            } else {
                dataError = 1;
                alert("Brightness settings of row " + rowNumber + " is not valid.");
                return;
            }
        } else if (type == "Set Value") {
            var b = {
                type: "Set Value",
                active: $this.find("input.active").is(':checked') ? 1 : 0,
                description: $this.find("input.description").val(),
                start: parseInt($this.find("input.start").val()),
                count: parseInt($this.find("input.count").val()),
                value: parseInt($this.find("input.value").val())
			};
            if ((b.start > 0) &&
                (b.count > 0)) {
                processors.push(b);
            } else {
                dataError = 1;
                alert("Set Value settings of row " + rowNumber + " is not valid.");
                return;
            }
        } else if (type == "Hold Value") {
            var b = {
                type: "Hold Value",
                active: $this.find("input.active").is(':checked') ? 1 : 0,
                description: $this.find("input.description").val(),
                start: parseInt($this.find("input.start").val()),
                count: parseInt($this.find("input.count").val()),
			};
            if ((b.start > 0) &&
                (b.count > 0)) {
                processors.push(b);
            } else {
                dataError = 1;
                alert("Hold Value settings of row " + rowNumber + " is not valid.");
                return;
            }
        } else if (type == "Reorder Colors") {
            var b = {
                type: "Reorder Colors",
                active: $this.find("input.active").is(':checked') ? 1 : 0,
                description: $this.find("input.description").val(),
                start: parseInt($this.find("input.start").val()),
                count: parseInt($this.find("input.count").val()),
                colorOrder: parseInt($this.find("select.colorOrder").val())
			};
            if ((b.start > 0) &&
                (b.count > 0)) {
                processors.push(b);
            } else {
                dataError = 1;
                alert("Color Order settings of row " + rowNumber + " is not valid.");
                return;
            }
        }


        rowNumber++;

	});

	if (dataError != 0)
		return;

	data.outputProcessors = processors;
    
	postData = "command=setOutputProcessors&data={ " + JSON.stringify(data) + " }";

	$.post("fppjson.php", postData).done(function(data) {
		$.jGrowl("Output Processors Table saved");
		PopulateOutputProcessorTable(data);
		SetRestartFlag(2);
	}).fail(function() {
		DialogError("Save Output Processors Table", "Save Failed");
	});
}

function AddOtherTypeOptions(row, type) {
    var config = "";
    
    if (type == "Remap") {
        config += "Source Channel: <input class='source' type=text  size='7' maxlength='7' value='1'/>&nbsp;"
                  + "Destination: <input class='destination' type=text size='7' maxlength='7' value='1'/>&nbsp;"
                  + "Count: <input class='count' type=text size='7' maxlength='7' value='1' />&nbsp;"
                  + "Loops: <input class='loops' type=text size='7' maxlength='7' value='1'/>&nbsp;"
                  + "Reverse: <select class='reverse'>"
				  + "<option value='0'>None</option>"
				  + "<option value='1'>By Channel</option>"
		          + "<option value='2'>RGB Pixels</option>"
		          + "<option value='3'>RGBW Pixels</option>"
		          + "</select>";
    } else if (type == "Brightness") {
        config += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='1'/>&nbsp;"
            + "Channel Count: <input class='count' type=text size='7' maxlength='7' value='1'/>&nbsp;"
            + "Brightness: <input class='brightness' type=number value='100' min='0' max='100'/>"
            + "Gamma: <input class='gamma' type=number step='0.1' value='1.0' min='0.1' max='5.0'/>";
    } else if (type == "Set Value") {
        config += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='1'/>&nbsp;"
            + "Channel Count: <input class='count' type=text size='7' maxlength='7' value='1'/>&nbsp;"
            + "Value: <input class='value' type=number value='255' min='0' max='255'/>";
    } else if (type == "Hold Value") {
        config += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='1'/>&nbsp;"
            + "Channel Count: <input class='count' type=text size='7' maxlength='7' value='1'/>";
    } else if (type == "Reorder Colors") {
        config += "Start Channel: <input class='start' type=text  size='7' maxlength='7' value='1'/>&nbsp;"
            + "Nodes: <input class='count' type=text size='7' maxlength='7' value='1'/>&nbsp;"
            + "Color Order: <select class='colorOrder'><option value='132'>132</option><option value='213'>213</option><option value='231'>231</option><option value='312'>312</option><option value='321'>321</option></select>";
    }

    
    row.find("td:nth-child(5)").html(config);
}
function ProcessorTypeSelected(selectbox) {
    $row = $(selectbox.parentNode.parentNode);
    var type = $(selectbox).val();
    $row.find("td:nth-child(3)").html(type);
    AddOtherTypeOptions($row, type);
}

function AddNewProcessorRow() {
	var currentRows = $("#outputProcessors > tbody > tr").length

	$('#outputProcessors tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
            "<td>" + (currentRows + 1) + "</td>" +
			"<td><input class='active' type='checkbox' checked></td>" +
            "<td><select class='type' onChange='ProcessorTypeSelected(this);'>" +
                 "<option value=''>Select a type</option>" +
                     "<option value='Remap'>Remap</option>" +
                     "<option value='Brightness'>Brightness</option>" +
                     "<option value='Hold Value'>Hold Value</option>" +
                     "<option value='Set Value'>Set Value</option>" +
                     "<option value='Reorder Colors'>Reorder Colors</option>" +
                  "</select></td>" +
			"<td><input class='description' type='text' size='32' maxlength='64' value=''></td>" +
            "<td> </td>" +
			"</tr>");
}

var tableInfo = {
	tableName: "outputProcessors",
	selected:  -1,
	enableButtons: [ "btnDelete" ],
	disableButtons: []
	};

function RenumberColumns(tableName) {
	var id = 1;
	$('#' + tableName + ' tbody tr').each(function() {
		$this = $(this);
		$this.find("td:first").html(id);
		id++;
	});
}
function DeleteSelectedProcessor() {
	if (tableInfo.selected >= 0) {
		$('#outputProcessors tbody tr:nth-child(' + (tableInfo.selected+1) + ')').remove();
		tableInfo.selected = -1;
		SetButtonState("#btnDelete", "disable");
        RenumberColumns("outputProcessors");
	}
}

$(document).ready(function(){
	SetupSelectableTableRow(tableInfo);
	GetOutputProcessors();
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
				<legend>Output Processors</legend>
				<table>
					<tr>
						<td width='70px'><input type=button value='Save' onClick='SetOutputProcessors();' class='buttons'></td>
						<td width='70px'><input type=button value='Add' onClick='AddNewProcessorRow();' class='buttons'></td>
						<td width='40px'>&nbsp;</td>
						<td width='70px'><input type=button value='Delete' onClick='DeleteSelectedProcessor();' id='btnDelete' class='disableButtons'></td>
					</tr>
				</table>
				<div class='fppTableWrapper fppTableWrapperAsTable'>
                    <div class='fppTableContents'>
                        <table id="outputProcessors">
                            <thead>
                                <tr>
                                    <th>#</td>
                                    <th>Active</td>
                                    <th>Type</th>
                                    <th>Description</th>
                                    <th>Configuration</th>
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
</body>
</html>
