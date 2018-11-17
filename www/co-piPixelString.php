<script>
function MapPixelStringType(type) {
    return type;
}
function MapPixelStringSubType(type) {
    return "";
}
function MapPixelStringSubTypeVersion(version) {
    return 1;
}
</script>

<?
include_once('co-pixelStrings.php');
?>


<script>


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
		str += pixelOutputTableRow(type, id, i, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
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


function populatePixelStringOutputs(data)
{
	$('#pixelOutputs').html("");
    
    if (data) {
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
            str += "<table id='" + type + "_Output_0' type='" + type + "' ports='2' class='outputTable'>";
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


	    if (output.outputCount == 0) {
                str += pixelOutputTableRow(type, id, 0, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
	    }
	    if (output.outputCount < 2) {
		str += pixelOutputTableRow(type, id, 1, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
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
}

function loadPixelStringOutputs()
{
	$.getJSON("fppjson.php?command=getChannelOutputs&file=co-pixelStrings", function(data) {
		populatePixelStringOutputs(data)
	});
}

function savePixelStringOutputs() {
    var postData = getPixelStringOutputJSON();
    
	// Double stringify so JSON in .json file is surrounded by { }
	postData = "command=setChannelOutputs&file=co-pixelStrings&data=" + JSON.stringify(JSON.stringify(postData));

	$.post("fppjson.php", postData).done(function(data) {
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
