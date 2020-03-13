<style>
.outputTable {
	background: #F0F0F0;
	width: 100%;
	border-spacing: 0px;
	border-collapse: collapse;
}

.outputTable th {
    vertical-align: bottom;
    text-align: center;
    border: solid 2px #888888;
    font-size: 0.8em;
}

.outputTable td {
	text-align: center;
    padding: 0px 9px 0px 0px ;
}

.outputTable tbody tr td input[type=text] {
	text-align: center;
    width: 100%;
}
.outputTable tbody tr td input[type=number] {
    text-align: center;
    width: 100%;
}

<?
if ($settings['Platform'] == "BeagleBone Black") {
    //  BBB only supports ws2811 at this point
    ?>
    #BBB48String tr > th:nth-of-type(2),
    #BBB48String tr > td:nth-of-type(2) {
        display: none;
    }
    <?
    }
    if ((isSet($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported")) {
        // don't support virtual strings
    ?>
    #BBB48String tr > th:nth-of-type(3),
    #BBB48String tr > td:nth-of-type(3) {
        display: none;
    }

    <?
    }
?>

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
    result += "<th>Protocol</th>";
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

function pixelOutputProtocolSelect(protocols, protocol)
{
    var result = "";
    var pixelProtocols = protocols.split(',');

    if (pixelProtocols.length == 0)
	pixelProtocols.push(protocol);

    result += "<select class='vsProtocol'>";

    for (i = 0; i < pixelProtocols.length; i++)
    {
        result += "<option value='" + pixelProtocols[i] + "'";
        if (protocol == pixelProtocols[i])
            result += " selected";
        result += ">" + pixelProtocols[i].toUpperCase() + "</option>";
    }

    result += "</select>";

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

function pixelOutputTableInputOrderOption(colorOrder, selectedColorOrder)
{
    var result = "<option value='";
    result += colorOrder;
    result += "'";
    if (colorOrder == selectedColorOrder)
        result += " selected";
    result += ">" + colorOrder + "</option>";
    return result;
}

function pixelOutputTableInputOrder(colorOrder)
{
    var result = "";
    
    result += "<td>";
    result += "<select class='vsColorOrder' onChange='updateItemEndChannel(this);'>";
    result += pixelOutputTableInputOrderOption('RGB', colorOrder);
    result += pixelOutputTableInputOrderOption('RBG', colorOrder);
    result += pixelOutputTableInputOrderOption('GBR', colorOrder);
    result += pixelOutputTableInputOrderOption('GRB', colorOrder);
    result += pixelOutputTableInputOrderOption('BGR', colorOrder);
    result += pixelOutputTableInputOrderOption('BRG', colorOrder);
    
    <?
    if ($settings['Platform'] == "BeagleBone Black")
    {
    ?>
        result += pixelOutputTableInputOrderOption('RGBW', colorOrder);
        result += pixelOutputTableInputOrderOption('RBGW', colorOrder);
        result += pixelOutputTableInputOrderOption('GBRW', colorOrder);
        result += pixelOutputTableInputOrderOption('GRBW', colorOrder);
        result += pixelOutputTableInputOrderOption('BGRW', colorOrder);
        result += pixelOutputTableInputOrderOption('BRGW', colorOrder);

        result += pixelOutputTableInputOrderOption('WRGB', colorOrder);
        result += pixelOutputTableInputOrderOption('WRBG', colorOrder);
        result += pixelOutputTableInputOrderOption('WGBR', colorOrder);
        result += pixelOutputTableInputOrderOption('WGRB', colorOrder);
        result += pixelOutputTableInputOrderOption('WBGR', colorOrder);
        result += pixelOutputTableInputOrderOption('WBRG', colorOrder);
    <?
    }
    ?>
    
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
    var protocols = row.attr('protocols');
    var protocol = row.find('.vsProtocol').val();
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
    
    str += pixelOutputTableRow(type, protocols, protocol, oid, pid, sid, desc + sid, 1, 0, 1, 0, 'RGB', 0, 0, 100, '1.0');
    
    $('#' + highestId).after(str);
}

function pixelOutputTableRow(type, protocols, protocol, oid, port, sid, description, startChannel, pixelCount, groupCount, reverse, colorOrder, nullCount, zigZag, brightness, gamma, portPfx = "")
{
    var result = "";
    var id = type + "_Output_" + oid + "_" + port + "_" + sid;
    
    result += "<tr id='" + id + "' type='" + type + "' oid='" + oid + "' pid='" + port + "' sid='" + sid + "' protocols='" + protocols + "'>";
    
    if (groupCount == 0) {
        groupCount = 1;
    }
    if (sid)
    {
        result += "<td>&nbsp;</td>";
        result += "<td><input type='hidden' class='vsProtocol' value='" + protocol + "'</td>";
        result += "<td><a href='#' ";
        result += "class='deleteButton' onClick='removeVirtualString(this);'></td>";
    }
    else
    {
        result += "<td class='vsPortLabel' align='center'>" + (port+1) + "" + portPfx + ")</td>";
        result += "<td>" + pixelOutputProtocolSelect(protocols, protocol) + "</td>";
        result += "<td><a href='#' ";
        result += "class='addButton' onClick='addVirtualString(this);'></td>";
    }
    
    result += "<td><input type='text' class='vsDescription' size='25' maxlength='60' value='" + description + "'></td>";
    result += "<td><input type='number' class='vsStartChannel' size='6' value='" + startChannel + "' min='1' max='<? echo FPPD_MAX_CHANNELS; ?>' onChange='updateItemEndChannel(this);' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
    result += "<td><input type='number' class='vsPixelCount' size='4' min='1' max='1600' value='" + pixelCount + "' onChange='updateItemEndChannel(this);' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
    result += "<td><input type='number' class='vsGroupCount' size='3' value='" + groupCount + "' min='1' max='1000' onChange='updateItemEndChannel(this);'></td>";
    if (groupCount == 0) {
        groupCount = 1;
    }
    result += "<td align='center' class='vsEndChannel'>" + (startChannel + (pixelCount * colorOrder.length)/groupCount - 1) + "</td>";
    result += pixelOutputTableInputDirection(reverse);
    result += pixelOutputTableInputOrder(colorOrder);
    result += "<td><input type='number' class='vsNullNodes' size='2' value='" + nullCount + "' min='0' max='100'></td>";
    result += "<td><input type='number' class='vsZigZag' size='3' value='" + zigZag + "' min='0' max='1600'></td>";
    result += pixelOutputTableInputBrightness(brightness);
    result += "<td><input type='number' class='vsGamma' size='5' value='" + gamma + "' min='0.1' max='5.0' step='0.01'></td>";
    result += "</tr>\n";
    
    return result;
}

function setPixelStringsStartChannelOnNextRow()
{
    if ($('#' + selectedPixelStringRowId).length)
    {
        var row = $('#' + selectedPixelStringRowId);
        var nextRow = row.closest('tr').next('tr');
        var startChannel = parseInt(row.find('.vsStartChannel').val());
        var pixelCount = parseInt(row.find('.vsPixelCount').val());
        var groupCount = parseInt(row.find('.vsGroupCount').val());
        if (groupCount == 0) {
            groupCount = 1;
        }
        var pixelType = row.find('.vsColorOrder').val();
        var chanPerNode = pixelType.length;
        var nextStart = startChannel + (chanPerNode * pixelCount)/groupCount;
        
        $('#pixelOutputs table tr').removeClass('selectedEntry');

        if (nextRow.html().indexOf('<hr>') != -1)
            nextRow = nextRow.next('tr');
        if (!nextRow.is(":visible")) {
            nextRow = nextRow.next('tr');
        }
            
        

        nextRow.find('.vsStartChannel').val(nextStart);
        nextRow.addClass('selectedEntry');
        updateRowEndChannel(nextRow);
        
        selectedPixelStringRowId = nextRow.attr('id');
    }
}

function updateRowEndChannel(row) {
    var startChannel = parseInt(row.find('.vsStartChannel').val());
    var pixelCount = parseInt(row.find('.vsPixelCount').val());
    var groupCount = parseInt(row.find('.vsGroupCount').val());
    var pixelType = row.find('.vsColorOrder').val();
    var chanPerNode = pixelType.length;
    
    if (groupCount == 0) {
        groupCount = 1;
    }
    var newEnd = startChannel + (chanPerNode * pixelCount)/groupCount - 1;
    if (pixelCount == 0) {
        newEnd = 0;
    }
    
    row.find('.vsEndChannel').html(newEnd);
}

function updateItemEndChannel(item) {
    var row = $(item).parent().parent();
    
    updateRowEndChannel(row);
}

function setRowData(row, protocol, description, startChannel, pixelCount, groupCount, reverse, colorOrder, nullNodes, zigZag, brightness, gamma)
{
    row.find('.vsProtocol').val(protocol);
    row.find('.vsDescription').val(description);
    row.find('.vsStartChannel').val(startChannel);
    row.find('.vsPixelCount').val(pixelCount);
    if (groupCount == 0) {
        groupCount = 1;
    }
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

    if (nextRow.find('td.vsPortLabel').length == 0)
        nextRow = nextRow.closest('tr').next('tr');
    
    row.find('.vsDescription').val(sDescription + '0');
    
    for (i = 0; i < clones; i++)
    {
        setRowData(nextRow,
                   row.find('.vsProtocol').val(),
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

        if (nextRow.find('td.vsPortLabel').length == 0)
            nextRow = nextRow.closest('tr').next('tr');
    }
}

function getPixelStringOutputJSON()
{
	var postData = {};
	postData.channelOutputs = [];

	$('#pixelOutputs table').each(function() {
		$this = $(this);
		var tableId = $this.attr('id');
		var enableId = tableId + '_enable';
		var output = {};

		output.type = MapPixelStringType($this.attr('type'));
        output.subType = MapPixelStringSubType($this.attr('type'));
        output.pinoutVersion = MapPixelStringSubTypeVersion($this.attr('type'));
		output.enabled = ($('#' + enableId).is(':checked')) ? 1 : 0;
		output.startChannel = 1;
		output.channelCount = -1;
		output.outputCount = parseInt($this.attr('ports'));

		var outputs = [];
		for (var i = 0; i < output.outputCount; i++) {
			var port = {};
            port.portNumber = i;
            port.protocol = $this.find('.vsProtocol').val();
            var dtId = (i & 0xFC);
            var dt = $('#DifferentialType' + dtId);
            var mxId = 1;
            if (dt === undefined) {
                mxId = 1;
            } else {
                mxId = dt.val();
                if (mxId === undefined) {
                    mxId = 1;
                } else {
                    mxId = parseInt(mxId);
                    port.differentialType = mxId;
                    if (mxId < 1) mxId = 1;
                }
            }
            var et = $('#ExpansionType' + i);
            if (et !== undefined) {
                var exptype = et.val();
                if (exptype === undefined) {
                    exptype = 1;
                } else {
                    exptype = parseInt(exptype);
                    port.expansionType = exptype;
                }
            }

            for (var  oid = 0; oid < mxId; oid++) {
                var virtualStrings = [];
                for (var j = 0; j < maxVirtualStringsPerOutput; j++) {
				    var id = output.type + "_Output_" + oid + "_" + i + "_" + j;

                    if ($('#' + id).length) {
                        var vs = {};

                        var row = $('#' + id);
                                  
                        if (!row.is(":visible")) {
                            vs.pixelCount = 0;
                            vs.description = "";
                            vs.startChannel = 0;
                            vs.groupCount = 0;
                            vs.reverse = 0;
                            vs.colorOrder = "RGB";
                            vs.nullNodes = 0;
                            vs.zigZag = 0;
                            vs.brightness = 100;
                            vs.gamma = "1.0";
                        } else {
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
                        }

                        virtualStrings.push(vs);
                    }
				}
                if (oid == 0) {
                    port.virtualStrings = virtualStrings;
                } else if (oid == 1) {
                    port.virtualStringsB = virtualStrings;
                } else if (oid == 2) {
                    port.virtualStringsC = virtualStrings;
                }
			}

			outputs.push(port);
        }

		output.outputs = outputs;
		postData.channelOutputs.push(output);
    });

    return postData;
}
    


</script>
