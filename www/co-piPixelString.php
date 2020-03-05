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

	var protocols = '';
	var protocol = '';

	var portCount = 0;
	if (type == 'RPIWS281X')
	{
		portCount = 2;
		protocol = 'ws281x';
		protocols = 'ws281x';
	}
	else if (type == 'SPI-WS2801')
	{
		portCount = 1;
		protocol = 'ws2801';
		protocols = 'ws2801';
	}
	else if (type == 'spixels')
	{
		portCount = 16;

		// Can't use ws2801 for now until mailbox issue is resolved.
		// spixels includes its own copy of the mailbox code, but
		// ends up calling the copy from jgarff's rpi-ws281x library
		// since the functions have the same names.  Do we fork and
		// rename or patch and submit a pull request?
		//protocol = 'ws2801';
		//protocols = 'ws2801,apa102,lpd6803,lpd8806';
		protocol = 'apa102';
		protocols = 'apa102,lpd6803,lpd8806';
	}
	else if (type == 'X11PixelStrings')
	{
		portCount = 32;
		protocol = 'X11';
		protocols = 'X11';
	}

	str += '<b>' + type + ' Output</b><br>';
	str += "Output Enabled: <input type='checkbox' id='" + type + "_Output_0_enable' checked><br>";

    str += "<div class='fppTableWrapper'>" +
        "<div class='fppTableContents'>";
	str += "<table id='" + type + "_Output_0' type='" + type + "' ports='" + portCount + "'>";
	str += pixelOutputTableHeader();
	str += "<tbody>";

	var id = 0; // FIXME if we need to handle multiple outputs of the same type
	var i = 0;
	for (i = 0; i < portCount; i++)
	{
		str += pixelOutputTableRow(type, protocols, protocol, id, i, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
	}

	str += "</tbody>";
	str += "</table>";
    str += "</div>";
    str += "</div>";

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
            var protocols = '';
            var protocol = '';

            if (type == 'RPIWS281X')
            {
                protocols = 'ws281x';

                if (protocol == '')
                    protocol = 'ws281x';
            }
            else if (type == 'SPI-WS2801')
            {
                protocols = 'ws2801';

                if (protocol == '')
                    protocol = 'ws2801';
            }
            else if (type == 'spixels')
            {
                protocols = 'ws2801,apa102,lpd6803,lpd8806';

                if (protocol == '')
                    protocol = 'ws2801';
            }
            else if (type == 'X11PixelStrings')
            {
                protocol = 'X11';
                protocols = 'X11';
            }

            str += "Output Enabled: <input type='checkbox' id='" + type + "_Output_0_enable'";

            if (output.enabled)
                str += " checked";

            str += "><br>";

            str += '<b>' + type + ' Output</b><br>';
            str += "<div class='fppTableWrapper'>" +
                "<div class='fppTableContents'>";
            str += "<table id='" + type + "_Output_0' type='" + type + "' ports='" + output.outputCount + "'>";
            str += pixelOutputTableHeader();
            str += "<tbody>";

            var id = 0; // FIXME if we need to handle multiple outputs of the same type

            for (var o = 0; o < output.outputCount; o++)
            {
                var port = output.outputs[o];

                if (port.protocol)
                    protocol = port.protocol;

                for (var v = 0; v < port.virtualStrings.length; v++)
                {
                    var vs = port.virtualStrings[v];

                    str += pixelOutputTableRow(type, protocols, protocol, id, o, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, vs.zigZag, vs.brightness, vs.gamma);
                }
            }


	    if (output.outputCount == 0) {
                str += pixelOutputTableRow(type, protocols, protocol, id, 0, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
	    }
	    if (output.outputCount < 2) {
		str += pixelOutputTableRow(type, protocols, protocol, id, 1, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
            }

            str += "</tbody>";
            str += "</table>";
            str += "</div>";
            str += "</div>";

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
		SetRestartFlag(1);
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
<?
if ($settings['Platform'] == "Raspberry Pi") {
?>
			<option value='RPIWS281X'>RPIWS281X</option>
			<option value='spixels'>spixels</option>
<!--
			<option value='SPI-WS2801'>SPI-WS2801</option>
-->
<? } else { ?>
			<option value='X11PixelStrings'>X11 Pixel Strings</option>
<?
}
?>
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
