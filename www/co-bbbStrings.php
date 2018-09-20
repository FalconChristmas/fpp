<style>
.serialOutputTable {
    background: #F0F0F0;
    border-spacing: 0px;
    border-collapse: collapse;
}
.serialOutputTable th {
    vertical-align: bottom;
}
.serialOutputTable td {
    text-align: center;
}
</style>

<script>
function MapPixelStringType(type) {
    return "BBB48String";
}
function MapPixelStringSubType(type) {
    return type;
}
function MapPixelStringSubTypeVersion(version) {
    return $('#BBB48StringSubTypeVersion').val();
}
</script>

<?
include_once('co-pixelStrings.php');
?>

<script>

var PixelStringLoaded = false;


function GetBBB48StringRequiresVersion() {
    <?
    if (strpos($settings['SubPlatform'], 'PocketBeagle') == FALSE)
    {
    ?>

    var subType = $('#BBB48StringSubType').val();
    if (subType == 'F8-B'
        || subType == 'F8-B-16'
        || subType == 'F8-B-20'
        || subType == 'F8-B-EXP'
        || subType == 'F8-B-EXP-32'
        || subType == 'F8-B-EXP-36') {
        return true;
    }
    <?
    }
    ?>
    return false;
}

function GetBBB48StringRows()
{
	var subType = $('#BBB48StringSubType').val();
	var rows = 48;

	if (subType == 'F16-B')
		rows = 16;
	else if (subType == 'F16-B-32')
		rows = 32;
    else if (subType == 'F16-B-40')
        rows = 40;
	else if (subType == 'F16-B-48')
		rows = 48;
	else if (subType == 'F4-B')
		rows = 4;
    else if (subType == 'F8-B')
        rows = 12;
    else if (subType == 'F8-B-16')
        rows = 16;
    else if (subType == 'F8-B-20')
        rows = 20;
    else if (subType == 'F8-B-EXP')
        rows = 28;
    else if (subType == 'F8-B-EXP-32')
        rows = 32;
    else if (subType == 'F8-B-EXP-36')
        rows = 36;
    else if (subType == 'F32-B')
        rows = 40;
    else if (subType == 'F32-B-48')
        rows = 48;
    else if (subType == 'RGBCape24')
        rows = 24;
	else if (subType == 'RGBCape48C')
		rows = 48;
    else if (subType == 'RGBCape48F')
        rows = 48;

	return rows;
}

function ShouldAddBreak(subType, s) {
    if (subType == 'F8-B-EXP') {
       if (s == 12 || s == 8) {
           return true;
       }
    } else if (subType == 'F8-B-EXP-32' && (s == 16 || s == 12 || s == 8)) {
        return true;
    } else if (subType == 'F8-B-EXP-36') {
        if (s == 20 || s == 16 || s == 12 || s == 8) {
            return true;
        }
    } else if (subType == 'F8-B-20' && (s == 16 || s == 12 || s == 8)) {
        return true;
    } else if (subType == 'F8-B-16' && (s == 12 || s == 8)) {
        return true;
    } else if (subType == 'F8-B' && s == 8) {
        return true;
    } else if (subType == 'F32-B' && s == 36) {
        return true;
    } else if (subType == 'F32-B-48' && (s == 36 || s == 40 || s == 44) ) {
        return true;
    } else if (s && ((s % 16) == 0)) {
        return true;
    }
}

function HasSerial(subType) {
    if ((subType == 'F16-B-48')
        || (subType == 'F8-B-20')
        || (subType == 'F8-B-EXP-36')
        || (subType == 'F32-B-48')
        || (subType == 'RGBCape24')
        || (subType == 'RGBCape48C')
        || (subType == 'RGBCape48F'))
    {
        return false;
    }
    return true;
}


function BBB48SerialTypeChanged() {
    if ($('#BBBSerialMode').val() == 'DMX') {
        $('#DMXNumChannelOutput').show();
        $('#BBBSerial_Output').show();
    } else if ($('#BBBSerialMode').val() == 'Pixelnet') {
        $('#DMXNumChannelOutput').hide();
        $('#BBBSerial_Output').show();
    } else {
        $('#DMXNumChannelOutput').hide();
        $('#BBBSerial_Output').hide();
    }
}

function SetupBBBSerialPorts()
{
	var subType = $('#BBB48StringSubType').val();

    
    if (HasSerial(subType)) {
        $('#BBBSerialOutputs').show();
    } else {
        $('#BBBSerialOutputs').hide();
    }
    
	if (subType == 'F4-B')
		maxPorts = 1;
    else if ((subType == 'F8-B-16') || (subType == 'F8-B-EXP-32'))
        maxPorts = 4;
	else
		maxPorts = 8;

	for (var i = 1; i <= 8; i++)
	{
		if (i <= maxPorts)
			$('#BBBSerialOutputRow' + i).show();
		else
			$('#BBBSerialOutputRow' + i).hide();
	}
}

function addSerialOutputJSON(postData) {
	var config = new Object();
	var startChannel = 999999;
	var endChannel = 1;

	config.type = "BBBSerial";
	config.enabled = 0;
	config.subType = $('#BBBSerialMode').val();
	config.startChannel = 0;
	config.channelCount = 0;
    config.device = $('#BBB48StringSubType').val();
	config.outputs = [];

	if (config.subType != 'off')
		config.enabled = 1;
    
    if (!$('#BBB48String_enable').is(":checked"))
        config.enabled = 0;
  
    if ($('#BBBSerialSelect').is(":hidden"))
        config.enabled = 0;



	var i = 1;
	for (i = 1; i <= 8; i++)
	{
		var output = new Object();
		var ai = '\\[' + i + '\\]';

		output.outputNumber = i - 1;
		output.startChannel = parseInt($('#BBBSerialStartChannel' + i).val());

		if (output.startChannel < startChannel)
			startChannel = output.startChannel;

		output.outputType = config.subType;

		if (config.subType == 'DMX')
            output.channelCount = parseInt($('#BBBSerialNumDMXChannels').val());
		else
			output.channelCount = 4096;

		var maxOutputChannel = output.startChannel + output.channelCount - 1;
		if (maxOutputChannel > endChannel)
			endChannel = maxOutputChannel;

		config.outputs.push(output);
	}

	config.startChannel = startChannel;
	config.channelCount = endChannel - startChannel + 1;

    postData.channelOutputs.push(config);
	return postData;
}

function populatePixelStringOutputs(data) {
    if (data) {
        for (var i = 0; i < data.channelOutputs.length; i++)
        {
            var output = data.channelOutputs[i];
            var type = output.type;
            if (type == 'BBB48String') {
                $('#BBB48String_enable').prop('checked', output.enabled);
                var subType = output.subType;
                $('#BBB48StringSubType').val(subType);
                var version = output.pinoutVersion;
                if (version == '2.x') {
                    $('#BBB48StringSubTypeVersion').val("2.x");
                } else {
                    $('#BBB48StringSubTypeVersion').val("1.x");
                }
                SetupBBBSerialPorts();
                
                if (GetBBB48StringRequiresVersion()) {
                    $('#BBB48StringSubTypeVersion').show();
                    $('#versionTag').show();
                } else {
                    $('#BBB48StringSubTypeVersion').hide();
                    $('#versionTag').hide();
                }

                $('#pixelOutputs').html("");
                
                var outputCount = GetBBB48StringRows();
                
                var str = "<table id='BBB48String' type='" + output.subType + "' ports='" + outputCount + "' class='outputTable'>";
                str += pixelOutputTableHeader();
                str += "<tbody>";
                var id = 0; // FIXME if we need to handle multiple outputs of the same type
                for (var o = 0; o < outputCount; o++)
                {
                    if (ShouldAddBreak(subType, o)) {
                        str += "<tr><td colSpan='13'><hr></td></tr>";
                    }
                    if (o < output.outputCount) {
                        var port = output.outputs[o];
                        for (var v = 0; v < port.virtualStrings.length; v++)
                        {
                            var vs = port.virtualStrings[v];
                        
                            str += pixelOutputTableRow(type, id, o, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, vs.zigZag, vs.brightness, vs.gamma);
                        }
                    } else {
                        str += pixelOutputTableRow(type, id, o, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
                    }
                }

                str += "</tbody>";
                str += "</table>";
                
                $('#pixelOutputs').append(str);
                
                $('#BBB48String').on('mousedown', 'tr', function(event, ui) {
                    $('#pixelOutputs table tr').removeClass('selectedEntry');
                    $(this).addClass('selectedEntry');
                    selectedPixelStringRowId = $(this).attr('id');
                });
            }
            if (type == 'BBBSerial') {
                var subType = output.subType;

                if (!HasSerial(output.device)) {
                    subType = "off";
                    $('#BBBSerialOutputs').hide();
                }
                $('#BBBSerialMode').val(subType);
                var outputs = output.outputs;
                if (subType == "DMX") {
                    if (outputs[0].channelCount > 0 && outputs[0].channelCount < 513) {
                        $('#BBBSerialNumDMXChannels').val(outputs[0].channelCount);
                    } else {
                        $('#BBBSerialNumDMXChannels').val("512");
                    }
                    $('#DMXNumChannelOutput').show();
                } else {
                    $('#BBBSerialNumDMXChannels').val("512");
                    $('#DMXNumChannelOutput').hide();
                }
                if (subType == "off")  {
                    $('#BBBSerial_Output').hide();
                } else {
                    $('#BBBSerial_Output').show();
                }
                if (outputs) {
                    for (var i = 0; i < outputs.length; i++)
                    {
                        var outputNumber = outputs[i].outputNumber + 1;
                        $('#BBBSerialStartChannel' + outputNumber).val(outputs[i].startChannel);
                    }
                }
            }
        }
    }
}

function BBB48StringSubTypeChanged()
{
    if (PixelStringLoaded) {
        $.getJSON("fppjson.php?command=getChannelOutputs&file=co-bbbStrings", function(data) {
                  for (var i = 0; i < data.channelOutputs.length; i++)
                  {
                    var output = data.channelOutputs[i];
                    var type = output.type;
                    if (data.channelOutputs[i].type == 'BBB48String') {
                        data.channelOutputs[i].subType = $('#BBB48StringSubType').val();
                    }
                    if (data.channelOutputs[i].type == 'BBBSerial') {
                      data.channelOutputs[i].device = $('#BBB48StringSubType').val();
                    }
                  }
                  populatePixelStringOutputs(data)
            });
    } else {
        var defaultData = {};
        defaultData.channelOutputs = [];
        var output = {};
        output.type = 'BBB48String';
        output.subType = $('#BBB48StringSubType').val();
        output.pinoutVersion = '1.x';
        defaultData.channelOutputs.push(output);
        populatePixelStringOutputs(defaultData);
    }
}

function loadBBBOutputs() {
    var defaultData = {};
    defaultData.channelOutputs = [];
    var output = {};
    output.type = 'BBB48String';
    output.subType = 'F8-B';
    defaultData.channelOutputs.push(output);

    var output = {};
    output.type = 'BBBSerial';
    output.device = 'F8-B';
    output.subType = 'off';
    defaultData.channelOutputs.push(output);
    
    populatePixelStringOutputs(defaultData);
    $.getJSON("fppjson.php?command=getChannelOutputs&file=co-bbbStrings", function(data) {
                PixelStringLoaded = true;
                populatePixelStringOutputs(data)
              });
}
function saveBBBOutputs() {
    var postData = getPixelStringOutputJSON();
    postData = addSerialOutputJSON(postData);
    
	// Double stringify so JSON in .json file is surrounded by { }
	postData = "command=setChannelOutputs&file=co-bbbStrings&data=" + JSON.stringify(JSON.stringify(postData));

	$.post("fppjson.php", postData).done(function(data) {
		$.jGrowl("Pixel String Output Configuration Saved");
		SetRestartFlag();
	}).fail(function() {
		DialogError("Save Pixel String Outputs", "Save Failed");
	});
}


$(document).ready(function(){
    loadBBBOutputs();
});
</script>

<div id='tab-BBB48String'>
	<div id='divBBB48String'>
		<fieldset class="fs">
			<legend> BeagleBone String Capes </legend>
			<div id='divBBB48StringData'>
				<div style="overflow: hidden; padding: 10px;">
					<table border=0 cellspacing=3>
						<tr>
							<td><b>Enable BBB String Cape:</b></td>
							<td><input id='BBB48String_enable' type='checkbox'></td>
							<td width=20>&nbsp;</td>
							<td width=20>&nbsp;</td>
							<td width=20>&nbsp;</td>
						</tr>
						<tr>
							<td><b>Cape Type:</b></td>
							<td colspan="3"><select id='BBB48StringSubType' onChange='BBB48StringSubTypeChanged();'>
								<option value='F8-B'>F8-B (8 serial)</option>
								<option value='F8-B-16'>F8-B (4 serial)</option>
								<option value='F8-B-20'>F8-B (No serial)</option>
								<option value='F8-B-EXP'>F8-B w/ Expansion (8 serial)</option>
								<option value='F8-B-EXP-32'>F8-B w/ Expansion (4 serial)</option>
								<option value='F8-B-EXP-36'>F8-B w/ Expansion (No serial)</option>
<?
    if (strpos($settings['SubPlatform'], 'PocketBeagle') == FALSE)
    {
?>
                                <option value='F16-B'>F16-B</option>
                                <option value='F16-B-32'>F16-B w/ 32 outputs</option>
                                <option value='F16-B-48'>F16-B w/ 48 outputs (No Serial)</option>
                                <option value='F4-B'>F4-B</option>
								<option value='F32-B'>F32-B</option>
								<option value='F32-B-48'>F32-B (No Serial)</option>
								<option value='RGBCape24'>RGBCape24</option>
								<option value='RGBCape48C'>RGBCape48C</option>
								<option value='RGBCape48F'>RGBCape48F</option>
<?
    }
?>
								</select>

							</td>
                            <td><b id='versionTag'>Version: </b></td>
                            <td><select id='BBB48StringSubTypeVersion'>
                                    <option value='1.x'>1.x</option>
                                    <option value='2.x'>2.x</option>
                                </select>
                            </td>
						</tr>
					</table>


                    <table style='width: 100%;'>
                        <tr>
                            <td align='left'>
                                <input type='button' onClick='cloneSelectedString();' value='Clone String'>
                                <input type='button' onClick='saveBBBOutputs();' value='Save'>
                                <input type='button' onClick='loadBBBOutputs();' value='Revert'>
                            </td>
                            <td align='right'>
                                Press F2 to auto set the start channel on the next row.
                            </td>
                        </tr>
                    </table>
                    <div id='pixelOutputs'>

                    </div>

					<br>
					<span id='BBBSerialOutputs'>
                        <table border=0 cellspacing=3>

						<tr id='BBBSerialSelect'>
							<td><b>Serial Mode:</b></td>
							<td><select id='BBBSerialMode' onChange='BBB48SerialTypeChanged();'>
									<option value='off'>Disabled</option>
									<option value='DMX'>DMX</option>
									<option value='Pixelnet'>Pixelnet</option>
								</select>
							</td>
							<td width=20>&nbsp;</td>
							<td><div id="DMXNumChannelOutput">Num&nbsp;DMX&nbsp;Channels:&nbsp;<input id='BBBSerialNumDMXChannels' size='6' maxlength='6' value='512'></div></td>
						</tr>
                    </table>
						<table ports='8' id='BBBSerial_Output' class='serialOutputTable'>
							<thead>
								<tr>
									<th width='30px'>#</th>
									<th width='90px'>Start<br>Channel</th>
								</tr>
							</thead>
                            <tbody>
							    <tr id='BBBSerialOutputRow1'>
                                    <td>1</td>
								    <td><input id='BBBSerialStartChannel1' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow2'>
                                    <td>2</td>
								    <td><input id='BBBSerialStartChannel2' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow3'>
                                    <td>3</td>
								    <td><input id='BBBSerialStartChannel3' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow4'>
                                    <td>4</td>
								    <td><input id='BBBSerialStartChannel4' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow5'>
                                    <td>5</td>
								    <td><input id='BBBSerialStartChannel5' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow6'>
                                    <td>6</td>
								    <td><input id='BBBSerialStartChannel6' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow7'>
                                    <td>7</td>
								    <td><input id='BBBSerialStartChannel7' size='6' maxlength='6' value='1'></td>
								</tr>
							    <tr id='BBBSerialOutputRow8'>
                                    <td>8</td>
								    <td><input id='BBBSerialStartChannel8' size='6' maxlength='6' value='1'></td>
								</tr>
                            </tbody>
						</table>
					</span>
				</div>
			</div>
		</fieldset>
	</div>
</div>
