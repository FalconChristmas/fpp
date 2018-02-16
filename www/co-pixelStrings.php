<style>
.outputTable {
	background: #F0F0F0;
	width: 100%;
	border-spacing: 0px;
	border-collapse: collapse;
}

.outputTable th {
	vertical-align: bottom;
}

.outputTable td {
	text-align: center;
}

.addButton {
	background-image: url(images/addicon.gif);
	width: 12px;
	height: 12px;
	display: block;
	background-size: 12px 12px;
	background-repeat: no-repeat;
}

.deleteButton {
	background-image: url(images/deleteicon.gif);
	width: 12px;
	height: 12px;
	display: block;
	background-size: 12px 12px;
	background-repeat: no-repeat;
}

#pixelOutputs input[type=text] {
	text-align: center;
	width: 100%;
}

</style>

<script>

var maxVirtualStringsPerOutput = 30;
var selectedPixelStringRowId = "NothingSelected";

function pixelOutputTableHeader()
{
	var result = "";

	result += "<thead>";
	result += "<tr>";
	result += "<th>Port</th>";
	result += "<th>&nbsp;</th>";
	result += "<th>Description</th>";
	result += "<th>Start<br>Channel</th>";
	result += "<th>Pixel<br>Count</th>";
	result += "<th>Group<br>Count</th>";
	result += "<th>End<br>Channel</th>";
	result += "<th>Direction</th>";
	result += "<th>Color<br>Order</th>";
	result += "<th>Null<br>Nodes</th>";
	result += "<th>Zig<br>Zag</th>";
	result += "<th>Bright-<br>ness</th>";
	result += "<th>Gamma</th>";
	result += "</tr>\n";
	result += "</thead>";

	return result;
}

function pixelOutputTableInputDirection(reverse)
{
	var result = "";

	result += "<td>";
	result += "<select class='vsReverse'>";
	result += "<option value='0'>Forward</option>";
	result += "<option value='1'";
	if (reverse)
		result += " selected";
	result += ">Reverse</option>";
	result += "</select>";
	result += "</td>";

	return result;
}

function pixelOutputTableInputOrder(colorOrder)
{
	var result = "";

	result += "<td>";
	result += "<select class='vsColorOrder'>";
	result += "<option value='RGB'>RGB</option>";
	result += "<option value='RBG'";
	if (colorOrder == 'RBG')
		result += " selected";
	result += ">RBG</option>";
	result += "<option value='GBR'";
	if (colorOrder == 'GBR')
		result += " selected";
	result += ">GBR</option>";
	result += "<option value='GRB'";
	if (colorOrder == 'GRB')
		result += " selected";
	result += ">GRB</option>";
	result += "<option value='BGR'";
	if (colorOrder == 'BGR')
		result += " selected";
	result += ">BGR</option>";
	result += "<option value='BRG'";
	if (colorOrder == 'BRG')
		result += " selected";
	result += ">BRG</option>";
	result += "</select>";
	result += "</td>";

	return result;
}

function pixelOutputTableInputBrightness(brightness)
{
	var result = "";

	result += "<td>";
	result += "<select class='vsBrightness'>";

	var i = 100;
	for (i = 100; i >= 5; i -= 5)
	{
		result += "<option value='" + i + "'";
		if (brightness == i)
			result += " selected";
		result += ">" + i + "%</option>";
	}

	result += "</select>";
	result += "</td>";

	return result;
}

function removeVirtualString(item)
{
	var tr = $(item).parent().parent();

	var id = tr.attr('id');

	// FIXME, do we need to do anything else other than this?
	tr.remove();
}

function addVirtualString(item)
{
	var row = $(item).parent().parent();
	var type = row.attr('type');
	var oid = parseInt(row.attr('oid'));
	var pid = parseInt(row.attr('pid'));
	var tbody = row.parent();
	var desc = row.find('.vsDescription').val();
	var str = "";

	// Find the highest string ID on this output
	var highest = 0;
	var i = maxVirtualStringsPerOutput;
	var highestId = "";
	for (i = maxVirtualStringsPerOutput; i >= 0; i--)
	{
		highestId = type + "_Output_" + oid + "_" + pid + "_" + i;
		if ($('#' + highestId).length)
		{
			highest = i;
			break;
		}
	}

	var sid = highest + 1;
	
	str += pixelOutputTableRow(type, oid, pid, sid, desc + sid, 1, 50, 1, 0, 'RGB', 0, 0, 100, '1.0');

	$('#' + highestId).after(str);
}

function pixelOutputTableRow(type, oid, port, sid, description, startChannel, pixelCount, groupCount, reverse, colorOrder, nullCount, zigZag, brightness, gamma)
{
	var result = "";
	var id = type + "_Output_" + oid + "_" + port + "_" + sid;

	result += "<tr id='" + id + "' type='" + type + "' oid='" + oid + "' + pid='" + port + "' sid='" + sid + "'>";

	if (sid)
	{
		result += "<td>&nbsp;</td>";
		result += "<td><a href='#' ";
		result += "class='deleteButton' onClick='removeVirtualString(this);'></td>";
	}
	else
	{
		result += "<td align='center'>" + (port+1) + ")</td>";
		result += "<td><a href='#' ";
		result += "class='addButton' onClick='addVirtualString(this);'></td>";
	}

	result += "<td><input type='text' class='vsDescription' size='30' value='" + description + "'></td>";
	result += "<td><input type='text' class='vsStartChannel' size='6' value='" + startChannel + "' onChange='updateItemEndChannel(this);' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
	result += "<td><input type='text' class='vsPixelCount' size='4' value='" + pixelCount + "' onChange='updateItemEndChannel(this);' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
	result += "<td><input type='text' class='vsGroupCount' size='3' value='" + groupCount + "'></td>";
	result += "<td align='center' class='vsEndChannel'>" + (startChannel + (pixelCount * 3) - 1) + "</td>";
	result += pixelOutputTableInputDirection(reverse);
	result += pixelOutputTableInputOrder(colorOrder);
	result += "<td><input type='text' class='vsNullNodes' size='2' value='" + nullCount + "'></td>";
	result += "<td><input type='text' class='vsZigZag' size='3' value='" + zigZag + "'></td>";
	result += pixelOutputTableInputBrightness(brightness);
	result += "<td><input type='text' class='vsGamma' size='5' value='" + gamma + "'></td>";
	result += "</tr>\n";

	return result;
}

function pixelStringInputsAreSane()
{
	// FIXME
	return 1;
}

function okToAddNewPixelStringInput(type)
{
	if ($('#' + type + '_Output_0').length)
	{
		alert('ERROR: You already have a ' + type + ' output created.');
		return 0;
	}

	return 1;
}

function addPixelOutput()
{
	var type = $('#pixelOutputType').val();

	if (!okToAddNewInput(type))
	{
		return;
	}

	var str = "<hr>\n";

	var portCount = 0;
	if (type == 'RPIWS281X')
		portCount = 2;
	else if (type == 'SPI-WS2801')
		portCount = 1;
	// FIXME, put BBB output port counts here

	str += '<b>' + type + ' Output</b><br>';
	str += "Output Enabled: <input type='checkbox' id='" + type + "_Output_0_enable' checked><br>";

	str += "<table id='" + type + "_Output_0' type='" + type + "' ports='" + portCount + "' class='outputTable'>";
	str += pixelOutputTableHeader();
	str += "<tbody>";

	var id = 0; // FIXME if we need to handle multiple outputs of the same type
	var i = 0;
	for (i = 0; i < portCount; i++)
	{
		str += pixelOutputTableRow(type, id, i, 0, '', 1, 50, 1, 0, 'RGB', 0, 0, 100, "1.0");
	}

	str += "</tbody>";
	str += "</table>";

	$('#pixelOutputs').append(str);

	$('#' + type + '_Output_0').on('mousedown', 'tr', function(event, ui) {
		$('#pixelOutputs table tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		selectedPixelStringRowId = $(this).attr('id');
	});
}

function setPixelStringsStartChannelOnNextRow()
{
	if ($('#' + selectedPixelStringRowId).length)
	{
		var row = $('#' + selectedPixelStringRowId);
		var nextRow = row.closest('tr').next('tr');
		var startChannel = parseInt(row.find('.vsStartChannel').val());
		var pixelCount = parseInt(row.find('.vsPixelCount').val());
		var nextStart = startChannel + (3 * pixelCount);
		var nextEnd = nextStart + (parseInt(nextRow.find('.vsPixelCount').val()) * 3) - 1;

		$('#pixelOutputs table tr').removeClass('selectedEntry');

		nextRow.find('.vsStartChannel').val(nextStart);
		nextRow.find('.vsEndChannel').html(nextEnd);
		nextRow.addClass('selectedEntry');
		selectedPixelStringRowId = nextRow.attr('id');
	}
}

function updateRowEndChannel(row) {
	var startChannel = parseInt(row.find('.vsStartChannel').val());
	var pixelCount = parseInt(row.find('.vsPixelCount').val());
	var newEnd = startChannel + (3 * pixelCount) - 1;

	row.find('.vsEndChannel').html(newEnd);
}

function updateItemEndChannel(item) {
	var row = $(item).parent().parent();

	updateRowEndChannel(row);
}

function setRowData(row, description, startChannel, pixelCount, groupCount, reverse, colorOrder, nullNodes, zigZag, brightness, gamma)
{
	row.find('.vsDescription').val(description);
	row.find('.vsStartChannel').val(startChannel);
	row.find('.vsPixelCount').val(pixelCount);
	row.find('.vsGroupCount').val(groupCount);
	row.find('.vsReverse').val(reverse);
	row.find('.vsColorOrder').val(colorOrder);
	row.find('.vsNullNodes').val(nullNodes);
	row.find('.vsZigZag').val(zigZag);
	row.find('.vsBrightness').val(brightness);
	row.find('.vsGamma').val(gamma);
	updateRowEndChannel(row);
}

function cloneSelectedString()
{
	var row = $('#' + selectedPixelStringRowId);
	var rowCount = row.parent().find('tr').length;
	var curRow = row.closest("tr")[0].rowIndex;
	var maxClone = rowCount - curRow;

	if (maxClone == 0)
	{
		alert("No strings below to clone.");
		return;
	}

	var clones = prompt('How many strings to clone from selected string?', maxClone);

	if (clones == null || clones == "")
		return;

	clones = parseInt(clones);

	var sDescription = row.find('.vsDescription').val();
	var sStartChannel = parseInt(row.find('.vsStartChannel').val());
	var sPixelCount = parseInt(row.find('.vsPixelCount').val());
	var nextRow = row.closest('tr').next('tr');

	row.find('.vsDescription').val(sDescription + '0');

	for (i = 0; i < clones; i++)
	{
		setRowData(nextRow,
			sDescription + (i+1),
			sStartChannel + (sPixelCount * 3 * (i+1)),
			sPixelCount,
			row.find('.vsGroupCount').val(),
			row.find('.vsReverse').val(),
			row.find('.vsColorOrder').val(),
			row.find('.vsNullNodes').val(),
			row.find('.vsZigZag').val(),
			row.find('.vsBrightness').val(),
			row.find('.vsGamma').val());
			
		nextRow = nextRow.closest('tr').next('tr');
	}
}

function populatePixelStringOutputs(data)
{
	$('#pixelOutputs').html("");

	for (var i = 0; i < data.channelOutputs.length; i++)
	{
		var output = data.channelOutputs[i];

		var type = output.type;
		var str = "<hr>\n";

		str += "Output Enabled: <input type='checkbox' id='" + type + "_Output_0_enable'";

		if (output.enabled)
			str += " checked";

		str += "><br>";

		str += '<b>' + type + ' Output</b><br>';
		str += "<table id='" + type + "_Output_0' type='" + type + "' ports='" + output.outputCount + "' class='outputTable'>";
		str += pixelOutputTableHeader();
		str += "<tbody>";

		var id = 0; // FIXME if we need to handle multiple outputs of the same type

		for (var o = 0; o < output.outputCount; o++)
		{
			var port = output.outputs[o];

			for (var v = 0; v < port.virtualStrings.length; v++)
			{
				var vs = port.virtualStrings[v];

				str += pixelOutputTableRow(type, id, o, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, vs.zigZag, vs.brightness, vs.gamma);
			}
		}

		str += "</tbody>";
		str += "</table>";

		$('#pixelOutputs').append(str);

		$('#' + type + '_Output_0').on('mousedown', 'tr', function(event, ui) {
			$('#pixelOutputs table tr').removeClass('selectedEntry');
			$(this).addClass('selectedEntry');
			selectedPixelStringRowId = $(this).attr('id');
		});
	}
}

function loadPixelStringOutputs()
{
	$.getJSON("fppjson.php?command=getChannelOutputs&file=co-pixelStrings", function(data) {
		populatePixelStringOutputs(data)
	});
}

function savePixelStringOutputs()
{
	var postData = {};
	var dataError = 0;

	postData.channelOutputs = [];

	$('#pixelOutputs table').each(function() {
		$this = $(this);

		var tableId = $this.attr('id');
		var enableId = tableId + '_enable';
		var output = {};

		output.type = $this.attr('type');
		output.enabled = ($('#' + enableId).is(':checked')) ? 1 : 0;
		output.subType = "";
		output.startChannel = 1;
		output.channelCount = -1;
		output.outputCount = parseInt($this.attr('ports'));

		var outputs = [];

		var i = 0;
		for (i = 0; i < output.outputCount; i++)
		{
			var port = {};
			var virtualStrings = [];
			var oid = 0; // FIXME, fix this if we can ever do more than one of same type

			// for blah
			for (var j = 0; j < maxVirtualStringsPerOutput; j++)
			{
				var id = output.type + "_Output_" + oid + "_" + i + "_" + j;

				if ($('#' + id).length)
				{
					var vs = {};

					var row = $('#' + id);

					vs.description = row.find('.vsDescription').val();
					vs.startChannel = parseInt(row.find('.vsStartChannel').val()) - 1;
					vs.pixelCount = parseInt(row.find('.vsPixelCount').val());
					vs.groupCount = parseInt(row.find('.vsGroupCount').val());
					vs.reverse = parseInt(row.find('.vsReverse').val());
					vs.colorOrder = row.find('.vsColorOrder').val();
					vs.nullNodes = parseInt(row.find('.vsNullNodes').val());
					vs.zigZag = parseInt(row.find('.vsZigZag').val());
					vs.brightness = parseInt(row.find('.vsBrightness').val());
					vs.gamma = row.find('.vsGamma').val();

					virtualStrings.push(vs);
				}
			}

			port.portNumber = i;
			port.virtualStrings = virtualStrings;
			outputs.push(port);
		}

		output.outputs = outputs;
		postData.channelOutputs.push(output);
	});

	if (dataError)
		return;

	// Double stringify so JSON in .json file is surrounded by { }
	postData = "command=setChannelOutputs&file=co-pixelStrings&data=" + JSON.stringify(JSON.stringify(postData));

	$.post("fppjson.php", postData).success(function(data) {
		$.jGrowl("Pixel String Output Configuration Saved");
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Pixel String Outputs", "Save Failed");
	});
}

$(document).ready(function() {
	loadPixelStringOutputs();
});

</script>

<div id='tab-PixelStrings'>
	<div id='divPixelStrings'>
		<b>New Type:</b>
		<select id='pixelOutputType'>
			<option value='RPIWS281X'>RPIWS281X</option>
<!--
			<option value='SPI-WS2801'>SPI-WS2801</option>
			<option value='BBB48String'>BBB (not implemented yet)</option>
-->
		</select>

		<input type='button' onClick='addPixelOutput();' value='Add Output'>
		<br>

		<table style='width: 100%;'>
			<tr>
				<td align='left'>
					<input type='button' onClick='cloneSelectedString();' value='Clone String'>
					<input type='button' onClick='savePixelStringOutputs();' value='Save'>
					<input type='button' onClick='loadPixelStringOutputs();' value='Revert'>
				</td>
				<td align='right'>
					Press F2 to auto set the start channel on the next row.
				</td>
			</tr>
		</table>
		<div id='pixelOutputs'>
		</div>
	</div>
</div>
