<script>
var BBB48StringOutputs = 48;

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

function GetBBB48StringConfig()
{
	var config = new Object();
	var startChannel = 999999;
	var endChannel = 1;

	config.type = "BBB48String";
	config.enabled = 0;
	config.subType = $('#BBB48StringSubType').val();
	config.startChannel = 0;
	config.channelCount = 0;
	config.outputCount = BBB48StringOutputs;
	config.outputs = [];

	if ($('#BBB48StringEnabled').is(":checked"))
		config.enabled = 1;

	var i = 0;
	for (i = 0; i < BBB48StringOutputs; i++)
	{
		var output = new Object();
		var ai = '\\[' + i + '\\]';

		output.portNumber = i;
		output.startChannel = parseInt($('#BBB48StartChannel' + ai).val());
		output.pixelCount = parseInt($('#BBB48PixelCount' + ai).val());
		output.colorOrder = $('#BBB48ColorOrder' + ai).val();
		output.nullNodes = parseInt($('#BBB48NullNodes' + ai).val());
		output.hybridMode = 0;
		output.reverse = parseInt($('#BBB48Direction' + ai).val());
		output.grouping = parseInt($('#BBB48Grouping' + ai).val());
		output.zigZag = parseInt($('#BBB48ZigZag' + ai).val());
        output.brightness = parseInt($('#BBB48Brightness' + ai).val());
        output.gamma = parseFloat($('#BBB48Gamma' + ai).val());
        
		if ($('#BBB48HybridMode' + ai).is(":checked"))
			output.hybridMode = 1;

		if (output.startChannel < startChannel)
			startChannel = output.startChannel;

		var maxOutputChannel = output.startChannel + ((output.nullNodes + output.pixelCount) * 3) - 1;
		if (maxOutputChannel > endChannel)
			endChannel = maxOutputChannel;

		config.outputs.push(output);
	}

	config.startChannel = startChannel;
	config.channelCount = endChannel - startChannel + 1;

	return config;
}

function GetColorOptionsSelect(id, selected)
{
	var options = ["RGB", "RBG", "GRB", "GBR", "BRG", "BGR"];
	var html = "";
	html += "<select id='" + id + "'>";

	var i = 0;
	for (i = 0; i < options.length; i++)
	{
		html += "<option value='" + options[i] + "'";

		if (options[i] == selected)
			html += " selected";

		html += ">" + options[i] + "</option>";
	}
	html += "</select>";

	return html;
}

function GetDirectionOptionsSelect(id, selected)
{
	var html = "";

    html += "<select id='" + id + "'>";
    
    html += "<option value='0'";
    if (selected == 0)
        html += " selected";

    html += ">Forward</option>";

    html += "<option value='1'";
    if (selected == 1)
        html += " selected";
    
    html += ">Reverse</option>";

    html += "</select>";
	return html;
}
function GetBrightnessOptionsSelect(id, selected)
{
    var html = "";
    html += "<select id='" + id + "'>";
    
    var i = 0;
    for (i = 100; i >= 0; )
    {
        html += "<option value='" + i + "'";
        if (selected == i)
            html += " selected";
        html += ">" + i + "%</option>";
        i = i - 5;
    }
    
    
    html += "</select>";
    return html;
}

function UpdateBBBStringEndChannel(row)
{
	var p = parseInt($('#BBB48PixelCount\\[' + row + '\\]').val());
	var sc = parseInt($('#BBB48StartChannel\\[' + row + '\\]').val());
	var gc = parseInt($('#BBB48Grouping\\[' + row + '\\]').val());

	if (gc == 0)
		gc = 1;

	$('#BBB48EndChannel\\[' + row + '\\]').val(sc - 1 + ((p / gc) * 3));
}

function DrawBBB48StringTable()
{
	var html = "";

	BBB48StringOutputs = GetBBB48StringRows();

	var subType = $('#BBB48StringSubType').val();

	if ((subType == 'F16-B-48')
        || (subType == 'F8-B-20')
        || (subType == 'F8-B-EXP-36')
        || (subType == 'F32-B-48')
        || (subType == 'RGBCape24')
        || (subType == 'RGBCape48C')
        || (subType == 'RGBCape48F'))
	{
		$('#BBBSerialSelect').hide();
		$('#BBBSerialOutputs').hide();
	}
	else
	{
		$('#BBBSerialSelect').show();
		$('#BBBSerialOutputs').show();
	}

	var s = 0;
	for (s = 0; s < BBB48StringOutputs; s++)
	{
        if (subType == 'F8-B-EXP') {
            if (s == 12 || s == 8) {
                html += "<tr><td colspan='12'><hr></td></tr>\n";
            }
        } else if (subType == 'F8-B-EXP-32' && (s == 16 || s == 12 || s == 8)) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F8-B-EXP-36') {
            if (s == 20 || s == 16 || s == 12 || s == 8) {
                html += "<tr><td colspan='12'><hr></td></tr>\n";
            }
        } else if (subType == 'F8-B-20' && (s == 16 || s == 12 || s == 8)) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F8-B-16' && (s == 12 || s == 8)) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F8-B' && s == 8) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F32-B' && s == 36) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (subType == 'F32-B-48' && (s == 36 || s == 40 || s == 44) ) {
            html += "<tr><td colspan='12'><hr></td></tr>\n";
        } else if (s && ((s % 16) == 0)) {
    		html += "<tr><td colspan='12'><hr></td></tr>\n";
		}

		html += "<tr id='BBB48StringRow" + s + "'>";
		html += "<td class='center'>" + (s+1) + "</td>";

		if (!("BBB48String" in channelOutputsLookup))
		{
			channelOutputsLookup["BBB48String"] = new Object();
			channelOutputsLookup["BBB48String"].outputs = [];
		}

		if (typeof channelOutputsLookup["BBB48String"].outputs[s] == 'undefined')
		{
			channelOutputsLookup["BBB48String"].outputs[s] = new Object();
			channelOutputsLookup["BBB48String"].outputs[s].portNumber = s;
			channelOutputsLookup["BBB48String"].outputs[s].startChannel = 1;
			channelOutputsLookup["BBB48String"].outputs[s].pixelCount = 0;
			channelOutputsLookup["BBB48String"].outputs[s].colorOrder = "RGB";
			channelOutputsLookup["BBB48String"].outputs[s].nullNodes = 0;
			channelOutputsLookup["BBB48String"].outputs[s].hybridMode = 0;
			channelOutputsLookup["BBB48String"].outputs[s].reverse = 0;
			channelOutputsLookup["BBB48String"].outputs[s].grouping = 0;
			channelOutputsLookup["BBB48String"].outputs[s].zigZag = 0;
            channelOutputsLookup["BBB48String"].outputs[s].brightness = 100;
            channelOutputsLookup["BBB48String"].outputs[s].gamma = 1.0;
		}

		var p = channelOutputsLookup["BBB48String"].outputs[s].pixelCount;
		var sc = channelOutputsLookup["BBB48String"].outputs[s].startChannel;
		var gc = channelOutputsLookup["BBB48String"].outputs[s].grouping;
        var bright = channelOutputsLookup["BBB48String"].outputs[s].brightness;
        var gamma = channelOutputsLookup["BBB48String"].outputs[s].gamma;

        if (bright == undefined) {
            bright = 100;
        }
        if (gamma == undefined) {
            gamma = 1.0;
        }

		if (gc == 0)
			gc = 1;

		var ec = sc - 1 + ((p / gc) * 3);

		html += "<td class='center'><input id='BBB48PixelCount[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].pixelCount + "' onChange='UpdateBBBStringEndChannel(" + s + ");'></td>";
		html += "<td class='center'><input id='BBB48StartChannel[" + s + "]' type='text' size='6' maxlength='6' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].startChannel + "' onChange='UpdateBBBStringEndChannel(" + s + ");'></td>";
		html += "<td class='center'><input id='BBB48EndChannel[" + s + "]' type='text' size='6' maxlength='6' value='" + ec + "' disabled=''></td>";
		html += "<td class='center'>" + GetColorOptionsSelect("BBB48ColorOrder[" + s + "]", channelOutputsLookup["BBB48String"].outputs[s].colorOrder) + "</td>";
		html += "<td class='center'>" + GetDirectionOptionsSelect("BBB48Direction[" + s + "]", channelOutputsLookup["BBB48String"].outputs[s].reverse) + "</td>";
		html += "<td class='center'><input id='BBB48Grouping[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].grouping + "' onChange='UpdateBBBStringEndChannel(" + s + ");'></td>";
		html += "<td class='center'><input id='BBB48NullNodes[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].nullNodes + "'></td>";

		html += "<td class='center'><input id='BBB48HybridMode[" + s + "]' type='checkbox'";
		if (channelOutputsLookup["BBB48String"].outputs[s].hybridMode)
			html += " checked";
		html += "></td>";

		html += "<td class='center'><input id='BBB48ZigZag[" + s + "]' type='text' size='3' maxlength='3' value='"
			+ channelOutputsLookup["BBB48String"].outputs[s].zigZag + "'></td>";
        
        
        html += "<td class='center'>" + GetBrightnessOptionsSelect("BBB48Brightness[" + s + "]", bright) + "</td>";
        html += "<td class='center'><input id='BBB48Gamma[" + s + "]' type='text' size='4' maxlength='4' value='" + gamma + "'></td>";


		html += "</tr>";
	}

	$('#BBB48StringTable tbody').html(html);
}

function BBB48StringSubTypeChanged()
{
	DrawBBB48StringTable();
	SetupBBBSerialStartChannels();
}
function BBB48SerialTypeChanged() {
    if ($('#BBBSerialMode').val() == 'DMX') {
        $('#DMXNumChannelOutput').show();
    } else {
        $('#DMXNumChannelOutput').hide();
    }
}

function InitializeBBB48String()
{
	if (("BBB48String-Enabled" in channelOutputsLookup) &&
			(channelOutputsLookup["BBB48String-Enabled"]))
		$('#BBB48StringEnabled').prop('checked', true);

	if (("BBB48String" in channelOutputsLookup) &&
		(typeof channelOutputsLookup["BBB48String"].subType != "undefined"))
	{
		if ((channelOutputsLookup["BBB48String"].subType == "F16-B") &&
				(channelOutputsLookup["BBB48String"].outputCount > 16))
		{
			if (channelOutputsLookup["BBB48String"].outputCount == 32)
				$('#BBB48StringSubType').val("F16-B-32");
			else if (channelOutputsLookup["BBB48String"].outputCount == 40)
				$('#BBB48StringSubType').val("F16-B-40");
			else
				$('#BBB48StringSubType').val("F16-B-48");
		}
		else
		{
			$('#BBB48StringSubType').val(channelOutputsLookup["BBB48String"].subType);
		}

	}

	DrawBBB48StringTable();
}

function InitializeBBBSerial()
{
	var maxPorts = 8;

	if (("BBB48String" in channelOutputsLookup) &&
		(typeof channelOutputsLookup["BBB48String"].subType != "undefined"))
	{
		if ((channelOutputsLookup["BBB48String"].subType == 'F16-B-48') ||
			(channelOutputsLookup["BBB48String"].subType == 'F8-B-20') ||
            (channelOutputsLookup["BBB48String"].subType == 'F8-B-EXP-36') ||
            (channelOutputsLookup["BBB48String"].subType == 'F32-B-48') ||
            (channelOutputsLookup["BBB48String"].subType == 'RGBCape24') ||
            (channelOutputsLookup["BBB48String"].subType == 'RGBCape48C') ||
            (channelOutputsLookup["BBB48String"].subType == 'RGBCape48F'))
			return; // nothing to setup if non-serial cape
	}

	if (("BBBSerial" in channelOutputsLookup) &&
		(typeof channelOutputsLookup["BBBSerial"].subType != "undefined") &&
		(channelOutputsLookup["BBBSerial"].subType != 'off'))
	{
		$('#BBBSerialMode').val(channelOutputsLookup["BBBSerial"].subType);
		var outputs = channelOutputsLookup["BBBSerial"].outputs;
        if (channelOutputsLookup["BBBSerial"].subType == 'DMX') {
            if (outputs[0].channelCount > 0 && outputs[0].channelCount < 513) {
                $('#BBBSerialNumDMXChannels').val(outputs[0].channelCount);
            } else {
                $('#BBBSerialNumDMXChannels').val("512");
            }
        } else {
            $('#BBBSerialNumDMXChannels').val("512");
        }

		for (var i = 0; i < outputs.length; i++)
		{
			var outputNumber = outputs[i].outputNumber + 1;
			$('#BBBSerialStartChannel' + outputNumber).val(outputs[i].startChannel);
		}
	}

	SetupBBBSerialStartChannels();
    BBB48SerialTypeChanged();
}

function SetupBBBSerialStartChannels()
{
	var subType = $('#BBB48StringSubType').val();

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

function GetBBBSerialConfig()
{
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
    
    if (!$('#BBB48StringEnabled').is(":checked"))
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

	return config;
}

$(document).ready(function(){
	InitializeBBB48String();
	InitializeBBBSerial();
});
</script>

<div id='tab-BBB48String'>
	<div id='divBBB48String'>
		<fieldset class="fs">
			<legend> BeagleBone Black String Cape </legend>
			<div id='divBBB48StringData'>
				<div style="overflow: hidden; padding: 10px;">
					<table border=0 cellspacing=3>
						<tr>
							<td><b>Enable BBB String Cape:</b></td>
							<td><input id='BBB48StringEnabled' type='checkbox'></td>
							<td width=20>&nbsp;</td>
							<td width=20>&nbsp;</td>
							<td width=20>&nbsp;</td>
						</tr>
						<tr>
							<td><b>Cape Type:</b></td>
							<td colspan="3"><select id='BBB48StringSubType' onChange='BBB48StringSubTypeChanged();'>
								<option value='F16-B'>F16-B</option>
								<option value='F16-B-32'>F16-B w/ 32 outputs</option>
								<option value='F16-B-48'>F16-B w/ 48 outputs (No Serial)</option>
								<option value='F4-B'>F4-B</option>
								<option value='F8-B'>F8-B (8 serial)</option>
								<option value='F8-B-16'>F8-B (4 serial)</option>
								<option value='F8-B-20'>F8-B (No serial)</option>
								<option value='F8-B-EXP'>F8-B w/ Expansion (8 serial)</option>
								<option value='F8-B-EXP-32'>F8-B w/ Expansion (4 serial)</option>
								<option value='F8-B-EXP-36'>F8-B w/ Expansion (No serial)</option>
								<option value='F32-B'>F32-B</option>
								<option value='F32-B-48'>F32-B (No Serial)</option>
								<option value='RGBCape24'>RGBCape24</option>
								<option value='RGBCape48C'>RGBCape48C</option>
								<option value='RGBCape48F'>RGBCape48F</option>
								</select>
							</td>
						</tr>
						<tr id='BBBSerialSelect'>
							<td><b>BBB Serial Cape Mode:</b></td>
							<td><select id='BBBSerialMode' onChange='BBB48SerialTypeChanged();'>
									<option value='off'>Disabled</option>
									<option value='DMX'>DMX</option>
									<option value='Pixelnet'>Pixelnet</option>
								</select>
							</td>
							<td width=20>&nbsp;</td>
							<td><div id="DMXNumChannelOutput">Num&nbsp;DMX&nbsp;Channels:&nbsp;<input id='BBBSerialNumDMXChannels' size='6' maxlength='6' value='512'></div></td>
						</tr>
						<tr>
							<td width = '70 px' colspan=5><input id='btnSaveChannelOutputsJSON' class='buttons' type='button' value='Save' onClick='SaveChannelOutputsJSON();'/> <font size=-1>(this will save changes to BBB tab &amp; LED Panels tab)</font></td>
						</tr>
					</table>
					<br>
					String Outputs:<br>
					<table id='BBB48StringTable'>
						<thead>
							<tr class='tblheader'>
								<td width='5%'>#</td>
								<td width='10%'>Node<br>Count</td>
								<td width='10%'>Start<br>Channel</td>
								<td width='10%'>End<br>Channel</td>
								<td width='5%'>RGB<br>Order</td>
								<td width='8%'>Direction</td>
								<td width='8%'>Group<br>Count</td>
								<td width='8%'>Null<br>Nodes</td>
								<td width='8%'>Hybrid</td>
								<td width='10%'>Zig<br>Zag</td>
								<td width='8%'>Brightness</td>
								<td width='8%'>Gamma</td>
							</tr>
						</thead>
						<tbody>
						</tbody>
					</table>
					<br>
					<span id='BBBSerialOutputs'>
					Serial Outputs:<br>

						<table>
							<thead>
								<tr class='tblheader'>
									<td width='30px'>#</td>
									<td width='90px'>Start<br>Channel</td>
								</tr>
							</thead>
							<tr id='BBBSerialOutputRow1'><td>1</td>
								<td><input id='BBBSerialStartChannel1' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow2'><td>2</td>
								<td><input id='BBBSerialStartChannel2' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow3'><td>3</td>
								<td><input id='BBBSerialStartChannel3' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow4'><td>4</td>
								<td><input id='BBBSerialStartChannel4' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow5'><td>5</td>
								<td><input id='BBBSerialStartChannel5' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow6'><td>6</td>
								<td><input id='BBBSerialStartChannel6' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow7'><td>7</td>
								<td><input id='BBBSerialStartChannel7' size='6' maxlength='6' value='1'></td>
								</tr>
							<tr id='BBBSerialOutputRow8'><td>8</td>
								<td><input id='BBBSerialStartChannel8' size='6' maxlength='6' value='1'></td>
								</tr>
						</table>
						
					</span>
				</div>
			</div>
		</fieldset>
	</div>
</div>
