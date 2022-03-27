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

h2.divider {
    width: 100%;
    text-align: left;
    border-bottom: 2px solid #000;
    line-height: 0.1em;
    margin: 12px 0 10px;
}

h2.divider span {
    background: #f5f5f5;
    padding: 0 10px;
}

/* prevent table header from scrolling: */
.fppTableContents {
    /* no-worky: max-height: calc(90% - 20px); */
    max-height: 400px; /* TODO: adjust for media queries? */
    overflow: auto;
}
table {
  text-align: left;
  position: relative; /* required for th sticky to work */
}
th {
    background: #fff; /* prevent add/delete circular buttons from showing through */
    position: sticky;
    top: 0;
    z-index: 20; /* draw table header on top of add/delete buttons */
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


#ModelPixelStrings_Output_0 tr > th:nth-of-type(2),
#ModelPixelStrings_Output_0 tr > td:nth-of-type(2) {
    display: none;
}
</style>




<script type="text/javascript">


var KNOWN_CAPES = {
<?
    function sortByLongName($a, $b) {
        return strcmp($a['longName'], $b['longName']);
    }
    
    function readCapes($cd, $capes) {
        global $settings;
        if (is_dir($cd)){
            if ($dh = opendir($cd)){
                while (($file = readdir($dh)) !== false){
                    $string = "";
                    if (substr($file, 0, 1) == '.') {
                        $string = "";
                    } else {
                        $string = file_get_contents($cd . $file);
//			echo "/* file len " . strlen($string) . "*/\n";
//			echo "/* ends with '" . substr($string, -4) . "' */\n";
                    }
                    
                    if ($string != "") {
                        $json = json_decode($string, true);
                        if ($json['numSerial'] != 0 && isSet($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported") {
                            // unsupported
                            continue;
                        } else if (empty($currentCape) || (isset($json['capes']) && in_array($currentCape, $json['capes']))) {
                            echo "'" . $file . "': " . $string . ",\n";
                            $file = str_replace('-v2.j', '.j', $file);
                            $file = str_replace('-v3.j', '.j', $file);
                            if (!isset($capes[$file])) {
                                $capes[$file] = $json;
                            }
                        }
                    }
                }
                closedir($dh);
            }
        }
        return $capes;
    }
    
    $capes = array();
    $capes = readCapes($mediaDirectory . "/tmp/strings/", $capes);
    if (count($capes) == 0 || $settings["showAllOptions"] == 1) {
        if ($settings['Platform'] == "Raspberry Pi") {
            $capedir = $fppDir . "/capes/pi/strings/";
        } else if ($settings['Platform'] == "BeagleBone Black") {
            $capedir = $fppDir . "/capes/bbb/strings/";
            if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
                $capedir = $fppDir . "/capes/pb/strings/";
            }
        } else {
            $capedir = $fppDir . "/capes/virtual/strings/";
        }
        $capes = readCapes($capedir, $capes);
    }
    usort($capes, 'sortByLongName');
?>
};

function MapPixelStringType(type) {
    var subType = GetBBB48StringCapeFileNameForSubType(type);
    if (KNOWN_CAPES[subType] && KNOWN_CAPES[subType].driver) {
        return KNOWN_CAPES[subType].driver;
    }
    return "BBB48String";
}
function MapPixelStringSubType(type) {
    return type;
}
function MapPixelStringSubTypeVersion(version) {
    return $('#BBB48StringSubTypeVersion').val();
}
function GetPixelStringTiming() {
    return $('#BBB48StringPixelTiming').val();
}

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
    result += "<th>Start<br>Nulls</th>";
    result += "<th>End<br>Nulls</th>";
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

    result += "<select class='vsProtocol'";
    if (pixelProtocols.length <= 1) {
      result += " style='display:none;'";
    }
    result += ">\n";

    for (i = 0; i < pixelProtocols.length; i++)
    {
        result += "<option value='" + pixelProtocols[i] + "'";
        if (protocol == pixelProtocols[i])
            result += " selected";
        result += ">" + pixelProtocols[i].toUpperCase() + "</option>";
    }

    result += "</select>";
    if (pixelProtocols.length == 1) {
        result += pixelProtocols[0];
    }
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
        result += pixelOutputTableInputOrderOption('W', colorOrder);
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
    var hwLabel = parseInt(row.attr('hwlabel'));
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
    
    str += pixelOutputTableRow(type, protocols, protocol, oid, pid, sid, desc + sid, 1, 0, 1, 0, 'RGB', 0, 0, 0, 100, '1.0', '', hwLabel);
    
    $('#' + highestId).after(str);
}

//try to standardize rowid:
//TODO: use this throughout, to make rowid format change easier
function rowid(type, outid, portid, strid) {
    return type + "_Output_" + outid + "_" + portid + "_" + strid;
}

function preventNonNumericalInput(e) {
    e = e || window.event;
    var charCode = (typeof e.which == "undefined") ? e.keyCode : e.which;
    var charStr = String.fromCharCode(charCode);

    if (!charStr.match(/^[0-9]+$/))
        e.preventDefault();
}

function pixelOutputTableRow(type, protocols, protocol, oid, port, sid, description, startChannel, pixelCount, groupCount, reverse, colorOrder, startNulls, endNulls, zigZag, brightness, gamma, portPfx = "", hwLabel = "")
{
    var result = "";
    var id = type + "_Output_" + oid + "_" + port + "_" + sid;
    
    if (hwLabel == "")
        hwLabel = "" + (port+1);
    
    result += "<tr id='" + id + "' type='" + type + "' oid='" + oid + "' pid='" + port + "' sid='" + sid + "' protocols='" + protocols + "' hwlabel='" + hwLabel +"'>";

    if (groupCount == 0) {
        groupCount = 1;
    }
    if (sid)
    {
        result += "<td>&nbsp;</td>";
        result += "<td><input type='hidden' class='vsProtocol' value='" + protocol + "'</td>";
        result += "<td><button ";
        result += "class='circularButton circularButton-sm circularVirtualStringButton circularDeleteButton' onClick='removeVirtualString(this);'></button></td>";
    }
    else
    {
        result += "<td class='vsPortLabel' align='center'>" + hwLabel + "" + portPfx + ")</td>";
        result += "<td>" + pixelOutputProtocolSelect(protocols, protocol) + "</td>";
        result += "<td ><button ";
        result += "class='circularButton circularButton-sm circularButton-visible circularVirtualStringButton circularAddButton' onClick='addVirtualString(this);'></button></td>";
    }
    
    result += "<td><input type='text' class='vsDescription' size='25' maxlength='60' value='" + description + "'></td>";
    result += "<td><input type='number' class='vsStartChannel' size='7' value='" + startChannel + "' min='1' max='<? echo FPPD_MAX_CHANNELS; ?>' onkeypress='preventNonNumericalInput(event)' onChange='updateItemEndChannel(this);' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
    result += "<td><input type='number' class='vsPixelCount' size='4' min='1' max='1600' onkeypress='preventNonNumericalInput(event)' value='" + pixelCount + "' onChange='updateItemEndChannel(this);' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
    result += "<td><input type='number' class='vsGroupCount' size='3' value='" + groupCount + "' min='1' max='1000' onkeypress='preventNonNumericalInput(event)' onChange='updateItemEndChannel(this);'></td>";
    if (groupCount == 0) {
        groupCount = 1;
    }
    result += "<td align='center' class='vsEndChannel'>" + (startChannel + (pixelCount * (colorOrder || "RGB").length)/groupCount - 1) + "</td>";
    result += pixelOutputTableInputDirection(reverse);
    result += pixelOutputTableInputOrder(colorOrder);
    result += "<td><input type='number' class='vsStartNulls' size='2' value='" + startNulls + "' min='0' max='100' onkeypress='preventNonNumericalInput(event)'></td>";
    result += "<td><input type='number' class='vsEndNulls' size='2' value='" + endNulls + "' min='0' max='100' onkeypress='preventNonNumericalInput(event)'></td>";
    result += "<td><input type='number' class='vsZigZag' size='3' value='" + zigZag + "' min='0' max='1600' onkeypress='preventNonNumericalInput(event)'></td>";
    result += pixelOutputTableInputBrightness(brightness);
    result += "<td><input type='number' class='vsGamma' size='3' value='" + gamma + "' min='0.1' max='5.0' step='0.01'></td>";
    result += "</tr>\n";
    
    return result;
}

function setPixelStringsStartChannelOnNextRow()
{
    if ($('#' + selectedPixelStringRowId).length)
    {
        var row = $('#' + selectedPixelStringRowId);
        var nextRow = row.closest('tr').next('tr');
        var startChannel = parseInt(row.find('.vsStartChannel').val()) || 1;
        var pixelCount = parseInt(row.find('.vsPixelCount').val()) || 0;
        var groupCount = parseInt(row.find('.vsGroupCount').val()) || 0;
        if (groupCount == 0) {
            groupCount = 1;
        }
        var pixelType = row.find('.vsColorOrder').val();
        var chanPerNode = (pixelType || "RGB").length;
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
    var startChannel = parseInt(row.find('.vsStartChannel').val()) || 1;
    var pixelCount = parseInt(row.find('.vsPixelCount').val()) || 0;
    var groupCount = parseInt(row.find('.vsGroupCount').val()) || 0;
    var pixelType = row.find('.vsColorOrder').val() || "RGB";
    var chanPerNode = (pixelType || "RGB").length;
    
    if (groupCount == 0) {
        groupCount = 1;
    }

    var stringChannels = chanPerNode * Math.ceil(pixelCount/groupCount);

    var newEnd = startChannel + stringChannels - 1;
    if (pixelCount == 0) {
        newEnd = 0;
    }
    
    row.find('.vsEndChannel').html(newEnd);
    selected_string_details(row);
}

function updateItemEndChannel(item) {
    var row = $(item).parent().parent();
    
    updateRowEndChannel(row);
}

//max pixel current:
//use current instead of power so 5V vs 12V doesn't matter
const pxAmps = {
    ws2811: 60e-3,
//add more types as needed
};

//show details about currently selected pixel port:
function selected_string_details(row) { //outputs, rowid) {
    let details = "";
    if (!row.hasClass("selectedEntry")) return; //don't overwrite with non-selected row (this happens when edited row loses focus > new row gets focus)
    const pins = (GetBBB48StringPins() || [])
        .map(hdr_pin => ({hdr_pin, gpio_info: (selected_string_details.gpio || []).find(pin_info => pin_info.pin == hdr_pin)}));
    const is_dpi = ~$('#BBB48StringSubType').val().indexOf("DPI");
    const portid = row.attr("id").replace(/_\d+$/, "");
    row = $('#' + portid + "_0"); //start with first string on this port
    const portinx = +(portid.match(/(?:_)(\d+)$/) || [])[1]; //some rows don't have port label so get it from row id
    const protocol = row.find(".vsProtocol").val(), pxA = pxAmps[protocol];
    const hdr_name = (pins[portinx] || {}).hdr_pin, gpio_name = ((pins[portinx] || {}).gpio_info || {}).gpio;
    if (is_dpi) {
//use on-screen values (might not be saved yet):
        let [numpx, maxA] = [0, 0];
        for (;;) //add all strings on this port
        {
            const sPixelCount = parseInt(row.find('.vsPixelCount').val()) || 0;
            const sNullNodes = parseInt(row.find('.vsNullNodes').val()) || 0;
            const sEndNulls = parseInt(row.find('.vsEndNulls').val()) || 0;
            const sBrightness = parseInt(row.find('.vsBrightness').val()) || 100;
	        numpx += sPixelCount + sNullNodes + sEndNulls;
            maxA += sPixelCount * sBrightness / 100 * pxA;
            row = row.closest('tr').next('tr');
            if ((row.attr("id") || "").replace(/_\d+$/, "") != portid) break;
        }
        const frtime = numpx? numpx * 30e-6 + 1e-3: 0; //allow 1 msec for refresh/latency, assume hsync/vsync don't add much overhead
        const port_fps = numpx? 1 / frtime: 0;
	    const cfg_fps = !numpx? "": //no output
            (port_fps < 20)? "OVERRUN": //will cause frame overrun
            (port_fps < 40)? "as 20": "as 40"; //TODO: add other fps if supported
        details += `<b>Port ${portinx + 1} (${(gpio_name !== null)? "GPIO" + gpio_name + " on ": ""}${hdr_name || "UNKNOWN PIN!"}):</b> ${plural(numpx)} pixel${plural()}, ${(frtime * 1e3).toFixed(3).replace(".000", "")} msec refresh${cfg_fps? ` (config ${cfg_fps} fps)`: ""}, ${maxA.toFixed(1).replace(".0", "")} A max`;
    }
    $("#pixel-string-details").html(details);
}

//keep the Grammar Police happy :)
function plural(n, multi, single) {
    if (n == null) return plural.suffix; //undef or null
    plural.suffix = (n != 1)? multi || "s": single || "";
    return n;
}

function setRowData(row, protocol, description, startChannel, pixelCount, groupCount, reverse, colorOrder, startNulls, endNulls, zigZag, brightness, gamma)
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
    row.find('.vsStartNulls').val(startNulls);
    row.find('.vsEndNulls').val(endNulls);
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
    
    if (clones == null || clones == "" || clones == 0)
        return;
    
    clones = parseInt(clones);
    const dir = (clones < 0)? "prev": "next";
    
    var sDescription = row.find('.vsDescription').val();
    var sStartChannel = parseInt(row.find('.vsStartChannel').val()) || 1;
    var sPixelCount = parseInt(row.find('.vsPixelCount').val()) || 0;
    var nextRow = row.closest('tr')[dir]('tr');

    if (nextRow.find('td.vsPortLabel').length == 0)
        nextRow = nextRow.closest('tr')[dir]('tr');
    
//    row.find('.vsDescription').val(sDescription + '0');
    const suffix = (sDescription.match(/(\d+)$/) || [])[1] || "0";

    for (i = 0; i < Math.abs(clones); i++)
    {
        const oldname = nextRow.find(".vsDescription").val();
        setRowData(nextRow,
                   row.find('.vsProtocol').val(),
//                   sDescription + (i+1),
                   oldname.match(/\d+$/)? oldname: oldname + (+suffix + i+1),
                   sStartChannel + (sPixelCount * 3 * (i+1)),
                   sPixelCount,
                   row.find('.vsGroupCount').val(),
                   row.find('.vsReverse').val(),
                   row.find('.vsColorOrder').val(),
                   row.find('.vsStartNulls').val(),
                   row.find('.vsEndNulls').val(),
                   row.find('.vsZigZag').val(),
                   row.find('.vsBrightness').val(),
                   row.find('.vsGamma').val());
        
        nextRow = nextRow.closest('tr')[dir]('tr');

        if (nextRow.find('td.vsPortLabel').length == 0)
            nextRow = nextRow.closest('tr')[dir]('tr');
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
        output.pixelTiming = parseInt(GetPixelStringTiming());

		var outputs = [];
		for (var i = 0; i < output.outputCount; i++) {
			var port = {};
            port.portNumber = i;
                                  
            var mainrow = $('#' + output.type + "_Output_0_" + i + "_0");
            port.protocol = mainrow.find('.vsProtocol').val();
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
                    mxCnt = parseInt($('#DifferentialCount' + dtId).val());
                    if (mxId == 1) {
                        mxId += mxCnt - 1;
                    } else if (mxId  == 2) {
                        mxId += mxCnt + 1;
                    }
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
                            vs.endNulls = 0;
                            vs.zigZag = 0;
                            vs.brightness = 100;
                            vs.gamma = "1.0";
                        } else {
                            vs.description = row.find('.vsDescription').val();
                            vs.startChannel = (parseInt(row.find('.vsStartChannel').val()) || 0) - 1;
                            vs.pixelCount = parseInt(row.find('.vsPixelCount').val()) || 0;
                            vs.groupCount = parseInt(row.find('.vsGroupCount').val()) || 0;
                            vs.reverse = parseInt(row.find('.vsReverse').val());
                            vs.colorOrder = row.find('.vsColorOrder').val();
                            vs.nullNodes = parseInt(row.find('.vsStartNulls').val()) || 0;
                            vs.endNulls =  parseInt(row.find('.vsEndNulls').val()) || 0;
                            vs.zigZag = parseInt(row.find('.vsZigZag').val()) || 0;
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
                } else if (oid == 3) {
                    port.virtualStringsD = virtualStrings;
                } else if (oid == 4) {
                    port.virtualStringsE = virtualStrings;
                } else if (oid == 5) {
                    port.virtualStringsF = virtualStrings;
                }
			}

			outputs.push(port);
        }

		output.outputs = outputs;
		postData.channelOutputs.push(output);
    });

    return postData;
}

var PixelStringLoaded = false;

function GetBBB48StringCapeFileNameForSubType(mainType) {
    var subType = "";
    var ver = $('#BBB48StringSubTypeVersion').val();
    if (ver == "2.x") {
        subType += "-v2";
    }
    if (ver == "3.x") {
        subType += "-v3";
    }
    subType += ".json";
    var type = mainType + subType;
    while (!KNOWN_CAPES[type]) {
        if (subType == "-v3.json") {
            subType = "-v2.json";
            ver = "2.x";
            $('#BBB48StringSubTypeVersion').val("2.x")
        } else if (subType == "-v2.json") {
            subType = ".json";
            $('#BBB48StringSubTypeVersion').val("1.x")
            ver = "1.x";
        } else {
            return type;
        }
        type = mainType + subType;
    }
    return type;
}
function IsPixelStringDriverType(type) {
    return  type == "BBB48String" || type == 'spixels' || type == 'RPIWS281X' || type == 'DPIPixels';
}
function GetBBB48StringCapeFileName() {
    var mainType = $('#BBB48StringSubType').val();
    if (!mainType || mainType == "") {
        var first = Object.keys(KNOWN_CAPES)[0];
        $('#BBB48StringSubType').val(KNOWN_CAPES[first].name);
        mainType = KNOWN_CAPES[first].name;
    }
    return GetBBB48StringCapeFileNameForSubType(mainType);
}


function GetBBB48StringRequiresVersion() {
    var mainType = $('#BBB48StringSubType').val();
    var v2SubType = mainType + "-v2.json";
    var v3SubType = mainType + "-v3.json";
    mainType += ".json";
    var count = 0;
    if (KNOWN_CAPES[mainType] != null)  {
        count++;
    }
    if (KNOWN_CAPES[v2SubType] != null)  {
        count++;
    }
    if (KNOWN_CAPES[v3SubType] != null)  {
        count++;
    }
    if (count <= 1) {
        return false;
    }
    return true;
}

function GetBBB48StringRows()
{
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    return (val["outputs"] || []).length;
}

//get array of header pin#s indexed by port#:
//NOTE: used by non-BBB capes as well
function GetBBB48StringPins()
{
    const subType = GetBBB48StringCapeFileName();
    const capeInfo = KNOWN_CAPES[subType];
    return capeInfo.outputs && (capeInfo.outputs || []).map(info => info.pin);
}

function GetBBB48StringProtocols(p) {
    var subType = GetBBB48StringCapeFileName();
    if (KNOWN_CAPES[subType]) {
        var val = KNOWN_CAPES[subType];
        if (val["outputs"][p]["protocols"]) {
            return val["outputs"][p]["protocols"].join(",");
        }
        if (val["protocols"]) {
            return val["protocols"].join(",");
        }
    }
    return "ws2811";
}
function GetBBB48StringDefaultProtocol(p) {
    var subType = GetBBB48StringCapeFileName();
    if (KNOWN_CAPES[subType]) {
        var val = KNOWN_CAPES[subType];
        if (val["outputs"][p]["protocols"]) {
            return val["outputs"][p]["protocols"][0];
        }
        if (val["protocols"]) {
            return val["protocols"][0];
        }
    }
    return "ws2811";
}
function GetStringHWLabel(p) {
    p += 1;
    var subType = GetBBB48StringCapeFileName();
    var group = {};
    if (KNOWN_CAPES[subType]) {
        var val = KNOWN_CAPES[subType];
        for (instance of val["groups"]) {
            if ((p >= instance['start']) &&
                (p < (instance['start'] + instance['count']))) {
                group = instance;
            }
        }
    }

    if (group.hasOwnProperty('portStart'))
        p = group['portStart'] + (p - group['start']);

    if (group.hasOwnProperty('portPrefix'))
        p = group['portPrefix'] + p;

    return p;
}
function GetGroupLabel(subType, s) {
    s = s + 1;
    var subType = GetBBB48StringCapeFileName();
    if (KNOWN_CAPES[subType]) {
        var val = KNOWN_CAPES[subType];
        for (instance of val["groups"]) {
            if ((s == instance["start"]) && instance.hasOwnProperty('label')) {
                return instance['label'];
            }
        }
    }
    return '';
}
function GetGroupPortStart(subType, s) {
    s = s + 1;
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    for (instance of val["groups"]) {
        if ((s == instance["start"]) && instance.hasOwnProperty('portStart')) {
            return instance['portStart'];
        }
    }
    return 0;
}
function ShouldAddBreak(subType, s) {
    s = s + 1;
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    for (instance of val["groups"]) {
        if (s == instance["start"]) {
            return true;
        }
    }
    return false;
}
function IsDifferential(subType, s) {
    s = s + 1;
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    for (instance of val["groups"]) {
        if (s == instance["start"]) {
            return instance["type"] == "differential";
        }
    }
    return false;
}
function IsExpansion(subType, s) {
    s = s + 1;
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    for (instance of val["groups"]) {
        if (s == instance["start"]) {
            return instance["type"] == "expansion";
        }
    }
    return false;
}
function IsDifferentialExpansion(isExpansion, expansionType, s) {
    if (isExpansion && expansionType == 1) {
        return (s % 4) == 0;
    }
    return false;
}

function HasSerial(subType) {
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    return val["numSerial"] > 0;
}


function BBB48SerialTypeChanged() {
    if ($('#BBBSerialMode').val() == 'DMX') {
        $('#DMXNumChannelOutput').show();
        ShowTableWrapper('BBBSerial_Output');
    } else if ($('#BBBSerialMode').val() == 'Pixelnet') {
        $('#DMXNumChannelOutput').hide();
        ShowTableWrapper('BBBSerial_Output');
    } else {
        $('#DMXNumChannelOutput').hide();
        HideTableWrapper('BBBSerial_Output');
    }
}

function SetupBBBSerialPorts()
{
    var subType = $('#BBB48StringSubType').val();

    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (HasSerial(subType)) {
        $('#BBBSerialOutputs').show();
        maxPorts = val["serial"].length;
        for (var i = 1; i <= 8; i++)
        {
            if (i <= maxPorts)
                $('#BBBSerialOutputRow' + i).show();
            else
                $('#BBBSerialOutputRow' + i).hide();
        }
    } else {
        $('#BBBSerialOutputs').hide();
    }

}

function addSerialOutputJSON(postData) {
    var config = new Object();
    var startChannel = 999999;
    var endChannel = 1;

    config.type = "BBBSerial";
    config.enabled = 0;
    config.pinoutVersion = MapPixelStringSubTypeVersion($this.attr('type'));
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


    var count = 0;
    var i = 1;
    for (i = 1; i <= 8; i++)
    {
        var output = new Object();
        var ai = '\\[' + i + '\\]';

        output.outputNumber = i - 1;
        output.startChannel = parseInt($('#BBBSerialStartChannel' + i).val()) || 0;
        if (output.startChannel > 0) {
            count = count + 1;
        }

        if (output.startChannel < startChannel)
            startChannel = output.startChannel;

        output.outputType = config.subType;

        if (config.subType == 'DMX')
            output.channelCount = parseInt($('#BBBSerialNumDMXChannels').val()) || 512;
        else
            output.channelCount = 4096;

        var maxOutputChannel = output.startChannel + output.channelCount - 1;
        if (maxOutputChannel > endChannel)
            endChannel = maxOutputChannel;

        config.outputs.push(output);
    }

    config.startChannel = startChannel;
    config.channelCount = endChannel - startChannel + 1;

    if (config.enabled == 0 || count == 0) {
        //add a hint to BBB48String that the serial isn't being used so it can
        //use the PRU if it wants to
        postData.channelOutputs[0].serialInUse = false;
    }
    postData.channelOutputs.push(config);
    return postData;
}
function BBB48StringExpansionTypeChanged(port) {
    var num = 16;
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    for (instance of val["groups"]) {
        if ((port+1) == instance["start"]) {
            if (instance["type"] == "expansion") {
                num = instance["count"];
            }
        }
    }
    var dt = $('#ExpansionType' + port);
    var val = parseInt(dt.val());
    var type = MapPixelStringType($('#BBB48StringSubType').val());

    if (val == 0 || val == -1) {
        //droping to standard/none... need to set everything to non-smart first
        for (var x = 0; x < num; x++) {
            BBB48StringDifferentialTypeChangedTo((port+x), 0, 0);
        }
        for (var x = 0; x < num; x++) {
            if ($('#ROW_RULER_DIFFERENTIAL_' + (port+x)).length) {
                var tr = $('#ROW_RULER_DIFFERENTIAL_' + (port+x));
                tr.remove();
            }
            if (val == -1) {
                $('#' + type + '_Output_0_' + (port+x) + '_0').hide();
            } else {
                $('#' + type + '_Output_0_' + (port+x) + '_0').show();
            }
        }
        
    } else {
        //going to differential, need to add receiver type selections
        for (var x = 0; x < num; x += 4) {
            $('#' + type + '_Output_0_' + (port+x) + '_0').show();
            $('#' + type + '_Output_0_' + (port+x+1) + '_0').show();
            $('#' + type + '_Output_0_' + (port+x+2) + '_0').show();
            $('#' + type + '_Output_0_' + (port+x+3) + '_0').show();
            var o = port + x;
            var str = "<tr id='ROW_RULER_DIFFERENTIAL_" +o + "'><td colSpan='3'><hr></td><td></td>";
            str += "<td colspan='2' style='font-size:0.7em; text-align:left; white-space: nowrap;'>Differential Receiver: ";

            str += "<select id='DifferentialType" + o + "' onChange='BBB48StringDifferentialTypeChanged(" + o + ");'>";
            str += "<option value='0' selected> Standard</option>";
            str += "<option value='1'>Smart v1</option>";
            str += "<option value='2'>Smart v2</option>";
            str += "</select>";
            str += "&nbsp;<select id='DifferentialCount" + o + "' onChange='BBB48StringDifferentialTypeChanged(" + o + ");' style='display: none;'>";
            str += "<option value='1' selected>1 Smart Receiver</option>";
            str += "<option value='2'>2 Smart Receivers</option>";
            str += "<option value='3'>3 Smart Receivers</option>";
            str += "</select>";
            str += "</td><td colSpan='9'><hr></td>";
            str += "</tr>";
            $('#' + type + '_Output_0_' + o + '_0').before(str);
        }
    }
}
function BBB48StringDifferentialTypeChanged(port) {
    var dt = $('#DifferentialType' + port);
    var val = parseInt(dt.val());
    var dc = $('#DifferentialCount' + port);
    var cval = parseInt(dc.val());
    if (val > 0) {
        var len = $('#DifferentialCount' + port + ' option').length;
        if (val == 1 && len > 3) {
            $("#DifferentialCount" + port + " option[value='4']").remove();
            $("#DifferentialCount" + port + " option[value='5']").remove();
            $("#DifferentialCount" + port + " option[value='6']").remove();
        } else if (val == 2 && len < 6) {
            dc.append('<option value="4">4 Smart Receivers</option>');
            dc.append('<option value="5">5 Smart Receivers</option>');
            dc.append('<option value="6">6 Smart Receivers</option>');
        }
        var len = $('#DifferentialCount' + port + ' option').length;
        if (cval >= len) {
            dc.val(len);
        }
        dc.show();
    } else {
        dc.hide();
    }
    var cval = parseInt(dc.val());
    if (val == 0 || cval == 0) {
        cval = 1;
    }
    BBB48StringDifferentialTypeChangedTo(port, val, cval);
}
function BBB48StringDifferentialTypeChangedTo(port, tp, count) {
    var protocols = GetBBB48StringProtocols(port);
    var protocol = GetBBB48StringDefaultProtocol(port);
    var type = MapPixelStringType($('#BBB48StringSubType').val());
    if (count == 0) {
        count = 1;
    }
    for (var x = count; x < 7; x++) {
        // remove the excess differentials
        if ($('#ROW_RULER_DIFFERENTIAL_' + port + '_' + x).length) {
            var tr = $('#ROW_RULER_DIFFERENTIAL_' + port + '_' + x);
            tr.remove();
        }
        for (var i = 0; i < 4; i++) {
            for (var j = 0; j < maxVirtualStringsPerOutput; j++) {
                var id = type + '_Output_' + x + '_' + (port + i) + '_' + j;
                if ($('#' + id).length) {
                    var row = $('#' + id);
                    row.remove();
                }
            }
        }
    }
    for (var x = 0; x < count; x++) {
        var label = "";
        if (tp >= 1) {
            if (x == 0) {
                label = "A";
            } else if (x == 1) {
                label = "B";
            } else if (x == 2) {
                label = "C";
            } else if (x == 3) {
                label = "D";
            } else if (x == 4) {
                label = "E";
            } else if (x == 5) {
                label = "F";
            }
        }
        // add rows if needed
        var tr = $('#ROW_RULER_DIFFERENTIAL_' + port + '_' + x);
        if (x > 0 && !tr.length) {
            var j = 0;
            for (var j = maxVirtualStringsPerOutput-1; j > 0; j--) {
                var id = type + '_Output_' + x + '_' + (port + 3) + '_' + j;
                if ($('#' + id).length) {
                    break;
                }
            }
            var str = "<tr id='ROW_RULER_DIFFERENTIAL_" + port + "_" + x + "'><td colSpan='15'><hr></td></tr>";
            for (var y = 0; y < 4; y++) {
                var hwLabel = GetStringHWLabel(port+y);
                str += pixelOutputTableRow(type, protocols, protocol, x, (port + y), 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 0, 100, "1.0", label, hwLabel);
            }
            $('#' + type + '_Output_' + (x-1) + '_' + (port + 3) + '_' + j).after(str);
        } else {
            for (var y = 0; y < 4; y++) {
                var newLabel = "" + (port + 1 + y) + label + ")";
                $('#' + type + '_Output_' + x + '_' + (port + y) + '_0 td:first').html(newLabel);
            }
        }
    }
}


function populatePixelStringOutputs(data) {
    if ("channelOutputs" in data) {
        for (var opi = 0; opi < data.channelOutputs.length; opi++) {
            var output = data.channelOutputs[opi];
            var type = output.type;
            if (IsPixelStringDriverType(type)) {
                $('#BBB48String_enable').prop('checked', output.enabled);
                var subType = output.subType;
                $('#BBB48StringSubType').val(subType);
                var version = output.pinoutVersion;
                if (version == '3.x') {
                    $('#BBB48StringSubTypeVersion').val("3.x");
                } else if (version == '2.x') {
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
                
                if (document.getElementById("BBB48StringSubType").length == 1) {
                    $('#BBB48StringSubType').hide();
                    document.getElementById("BBB48StringSubTypeSpan").textContent = subType;
                    $('#BBB48StringSubTypeSpan').show();
                } else {
                    $('#BBB48StringSubType').show();
                    $('#BBB48StringSubTypeSpan').hide();
                }
                
                
                var pixelTiming = 0;
                if ('pixelTiming' in output) {
                    pixelTiming = output.pixelTiming;
                }
                $('#BBB48StringPixelTiming').val(pixelTiming);


                $('#pixelOutputs').html("");
                
                var outputCount = GetBBB48StringRows();

                var str = "";
                str += "<div class='fppTableWrapper'>" +
                    "<div class='fppTableContents' role='region' aria-labelledby='BBB48String' tabindex='0'>";
                str += "<table id='BBB48String' class='fppSelectableRowTable' type='" + output.subType + "' ports='" + outputCount + "'>";
                str += pixelOutputTableHeader();
                str += "<tbody>";

                var expansions = [];
                var expansionType = 0;
                var inExpansion = false;
                var sourceOutputCount = output.outputCount;
                if (output.outputs != null) {
                    sourceOutputCount = output.outputs.length;
                }
                
                
                for (var o = 0; o < outputCount; o++)
                {
                    var protocols = GetBBB48StringProtocols(o);
                    var defProtocol = GetBBB48StringDefaultProtocol(o);
                    var port = {"differentialType" : 0, "expansionType" : 0};
                    var loops = 1;
                    if (o < sourceOutputCount) {
                        port = output.outputs[o];
                    }
                    if (ShouldAddBreak(subType, o) || (o == 0 && IsDifferential(subType, o)) || IsDifferentialExpansion(inExpansion, expansionType, o) || IsExpansion(subType, o)) {
                        var groupLabel = GetGroupLabel(subType, o);
                        if (groupLabel != '') {
                            str += "<tr><td colspan='15'><h2 class='divider'><span>" + groupLabel + "</span></h2></td></tr>";
                        }
                        if (IsExpansion(subType, o)) {
                            expansionType = port["expansionType"];
                            if (expansionType == null) {
                                expansionType = data["defaultExpansionType"];
                                if (expansionType == null) {
                                    expansionType = 0;
                                }
                            }
                            str += "<tr><td colSpan='2'><hr></td>";
                            str += "<td></td>";
                            str += "<td style='font-size:0.7em; text-align:left; white-space: nowrap;'>Expansion Type: ";
                            
                            
                            str += "<select id='ExpansionType" + o + "' onChange='BBB48StringExpansionTypeChanged(" + o + ");'>";
                            str += "<option value='-1'" + (expansionType == -1 ? " selected" : "") + ">None</option>";
                            str += "<option value='0'" + (expansionType == 0 ? " selected" : "") + ">Standard</option>";
                            str += "<option value='1'" + (expansionType == 1 ? " selected" : "") + ">Differential</option>";
                            str += "</select></td><td colSpan='11'><hr></td>";
                            str += "</tr>";
                            if (expansionType == -1) {
                                expansions.push(o);
                            }
                            inExpansion = true;
                        }
                        if (IsDifferential(subType, o) || IsDifferentialExpansion(inExpansion, expansionType, o)) {
                            str += "<tr id='ROW_RULER_DIFFERENTIAL_" + o + "'><td colSpan='2'><hr></td>";
                            str += "<td></td>";
                            str += "<td colspan='3' style='font-size:0.7em; text-align:left; white-space: nowrap;'>Differential Receiver: ";


                            var diffType = 0;
                            if (port.hasOwnProperty("differentialType"))
                                diffType = port["differentialType"];

                            var diffCount = 0;
                            if (diffType > 1 && diffType < 4) {
                                diffCount = diffType;
                                diffType = 1;
                            } else if (diffType > 3) {
                                diffCount = diffType - 3
                                diffType = 2;
                            }
                            str += "<select id='DifferentialType" + o + "' onChange='BBB48StringDifferentialTypeChanged(" + o + ");'>";
                            str += "<option value='0'" + (diffType == 0 ? " selected" : "") + ">Standard</option>";
                            str += "<option value='1'" + (diffType == 1 ? " selected" : "") + ">Smart v1</option>";
                            str += "<option value='2'" + (diffType == 2 ? " selected" : "") + ">Smart v2</option>";
                            str += "</select>";
                            str += "&nbsp;<select id='DifferentialCount" + o + "' onChange='BBB48StringDifferentialTypeChanged(" + o + ");'";
                            if (diffType == 0) {
                                str +=  "style=' display: none;'";
                            }
                            str += ">"
                            str += "<option value='1'" + (diffCount <= 1 ? " selected" : "") + ">1 Smart Receiver</option>";
                            str += "<option value='2'" + (diffCount == 2 ? " selected" : "") + ">2 Smart Receivers</option>";
                            str += "<option value='3'" + (diffCount == 3 ? " selected" : "") + ">3 Smart Receivers</option>";
                            if (diffType == 2) {
                                str += "<option value='4'" + (diffCount == 4 ? " selected" : "") + ">4 Smart Receivers</option>";
                                str += "<option value='5'" + (diffCount == 5 ? " selected" : "") + ">5 Smart Receivers</option>";
                                str += "<option value='6'" + (diffCount == 6 ? " selected" : "") + ">6 Smart Receivers</option>";
                            }
                            str += "</select>";

                            str += "</td><td colSpan='9'><hr></td>";
                            str += "</tr>";
                            
                            if (diffType >= 1) {
                                loops = diffCount;
                            }
                        } else if ((o != 0) && (!inExpansion)) {
                            str += "<tr><td colSpan='15'><hr></td></tr>";
                        }
                    }
                    if (loops > 1) {
                        var protocol = defProtocol;
                        if (port["protocol"]) {
                            protocol = port["protocol"];
                        }

                        for (var l = 0; l < loops; l++) {
                            var pfx = "A";
                            if (l == 1) {
                                pfx = "B";
                            } else if (l == 2) {
                                pfx = "C";
                            } else if (l == 3) {
                                pfx = "D";
                            } else if (l == 4) {
                                pfx = "E";
                            } else if (l == 5) {
                                pfx = "F";
                            }
                            if (l != 0) {
                                str += "<tr id='ROW_RULER_DIFFERENTIAL_" + o + "_" + l + "'><td colSpan='15'><hr></td></tr>";
                            }

                            for (var sr = 0; sr < 4; sr++) {
                                var o2 = o + sr;
                                if (o2 < sourceOutputCount) {
                                    var port = output.outputs[o2];
                                    var strings = port.virtualStrings;
                                    if (l == 1) {
                                        strings = port.virtualStringsB;
                                    } else if (l == 2) {
                                        strings = port.virtualStringsC;
                                    } else if (l == 3) {
                                        strings = port.virtualStringsD;
                                    } else if (l == 4) {
                                        strings = port.virtualStringsE;
                                    } else if (l == 5) {
                                        strings = port.virtualStringsF;
                                    }
                                    
                                    if (strings != null && strings.length > 0) {
                                        for (var v = 0; v < strings.length; v++) {
                                            var vs = strings[v];
                                            
                                            var endNulls = vs.hasOwnProperty("endNulls") ? vs.endNulls : 0;
                                            var hwLabel = GetStringHWLabel(o2);
                                            str += pixelOutputTableRow(type, protocols, protocol, l, o2, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, endNulls, vs.zigZag, vs.brightness, vs.gamma, pfx, hwLabel);
                                        }
                                    } else {
                                        var hwLabel = GetStringHWLabel(o2);
                                        str += pixelOutputTableRow(type, protocols, protocol, l, o2, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 0, 100, "1.0", pfx, hwLabel);
                                    }
                                } else {
                                    var hwLabel = GetStringHWLabel(o2);
                                    str += pixelOutputTableRow(type, protocols, protocol, l, o2, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 0, 100, "1.0", pfx, hwLabel);
                                }
                            }
                        }
                        o+= 3;
                    } else {
                        if (o < sourceOutputCount && output.outputs[o].virtualStrings != null) {
                            var port = output.outputs[o];
                            for (var v = 0; v < port.virtualStrings.length; v++) {
                                var vs = port.virtualStrings[v];
                            
                                var protocol = defProtocol;
                                if (port["protocol"]) {
                                    protocol = port["protocol"];
                                }
                                var endNulls = vs.hasOwnProperty("endNulls") ? vs.endNulls : 0;
                                var hwLabel = GetStringHWLabel(o);
                                str += pixelOutputTableRow(type, protocols, protocol, 0, o, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, endNulls, vs.zigZag, vs.brightness, vs.gamma, '', hwLabel);
                            }
                        } else {
                            var hwLabel = GetStringHWLabel(o);
                            str += pixelOutputTableRow(type, protocols, defProtocol, 0, o, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 0, 100, "1.0", '', hwLabel);
                        }
                    }
                }

                str += "</tbody>";
                str += "</table>";
                str += "</div>";
                str += "</div>";
                
                $('#pixelOutputs').append(str);
                
                expansions.forEach(function(r) {
                                   BBB48StringExpansionTypeChanged(r);
                                   });
                
                $('#BBB48String').on('mousedown', 'tr', function(event, ui) {
                    $('#pixelOutputs table tr').removeClass('selectedEntry');
                    $(this).addClass('selectedEntry');
                    selectedPixelStringRowId = $(this).attr('id');
                    selected_string_details($(this)); //output.outputs, selectedPixelStringRowId);
                });

                var key = GetBBB48StringCapeFileNameForSubType(subType);
                var val = KNOWN_CAPES[key];
                if (val.hasOwnProperty('notes')) {
                    $('.capeNotes').show();
                    $('#capeNotes').html(val.notes);
                } else {
                    $('.capeNotes').hide();
                    $('#capeNotes').html('');
                }
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
                    HideTableWrapper('BBBSerial_Output');
                } else {
                    ShowTableWrapper('BBBSerial_Output');
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
function ValidateBBBStrings(data) {
    if ("channelOutputs" in data) {
        PixelStringLoaded = true;
        for (var i = 0; i < data.channelOutputs.length; i++) {
            var output = data.channelOutputs[i];
            var type = output.type;
            if (IsPixelStringDriverType(type)) {
                if (output.subType == "") {
                    if (output.type == "RPIWS281X") {
                        output.subType = "PiHat";
                    }
                    if (output.type == "spixels") {
                        output.subType = "spixels";
                    }
                    if (output.type == "DPIPixels") {
                        output.subType = "DPIPixels-24";
                    }
                }
                var fn = GetBBB48StringCapeFileNameForSubType(output.subType);
                if (KNOWN_CAPES[fn] == null) {
                    fn = KNOWN_CAPES[Object.keys(KNOWN_CAPES)[0]];
                    output.subType = fn.name;
                    if (typeof fn.pinoutVersion !== 'undefined') {
                        output.pinoutVersion = fn.pinoutVersion;
                    }
                } else {
                    fn = KNOWN_CAPES[fn];
                    if (typeof fn.pinoutVersion !== 'undefined') {
                        output.pinoutVersion = fn.pinoutVersion;
                    }
                }
            } else if (type == 'BBBSerial') {
                var fn = GetBBB48StringCapeFileNameForSubType(output.device);
                if (KNOWN_CAPES[fn] == null) {
                    fn = KNOWN_CAPES[Object.keys(KNOWN_CAPES)[0]];
                    output.device = fn.name;
                    if (typeof fn.pinoutVersion !== 'undefined') {
                        output.pinoutVersion = fn.pinoutVersion;
                    }
                } else {
                    fn = KNOWN_CAPES[fn];
                    if (typeof fn.pinoutVersion !== 'undefined') {
                        output.pinoutVersion = fn.pinoutVersion;
                    }
                }
            }
        }
    }
}

<? if ($settings['Platform'] == "BeagleBone Black") { ?>
var PIXEL_STRING_FILE_NAME = "co-bbbStrings";
<? } else { ?>
var PIXEL_STRING_FILE_NAME = "co-pixelStrings";
<? } ?>

function BBB48StringSubTypeChanged()
{
    if (PixelStringLoaded) {
        $.getJSON("api/channel/output/" + PIXEL_STRING_FILE_NAME, function(data) {
                  for (var i = 0; i < data.channelOutputs.length; i++) {
                    if (IsPixelStringDriverType(data.channelOutputs[i].type)) {
                        data.channelOutputs[i].type = MapPixelStringType($('#BBB48StringSubType').val());
                        data.channelOutputs[i].subType = $('#BBB48StringSubType').val();
                    }
                    if (data.channelOutputs[i].type == 'BBBSerial') {
                      data.channelOutputs[i].device = $('#BBB48StringSubType').val();
                    }
                  }
                  ValidateBBBStrings(data);
                  populatePixelStringOutputs(data)
            });
    } else {
        var defaultData = {};
        defaultData.channelOutputs = [];
        var output = {};
        output.type = MapPixelStringType($('#BBB48StringSubType').val());
        output.subType = $('#BBB48StringSubType').val();
        <?
        if (isset($capes[0]['pinoutVersion'])) {
            echo 'output.pinoutVersion = "' . $capes[0]['pinoutVersion'] . '";';
        } else {
            ?>
            output.pinoutVersion = "1.x";
            <?
        }
        ?>
        defaultData.channelOutputs.push(output);
        populatePixelStringOutputs(defaultData);
    }
}

//get uploaded xLights models:
<?
define("xLights_MODELS", "/home/fpp/media/upload/xlights_rgbeffects.xml");
$models_err = "";
clearstatcache(true); //TODO: is this needed?
$models_str = file_get_contents(xLights_MODELS);
if ($models_str == "") $models_err = "xlights_rgbeffect.xml not found.  Please upload it from your xLights folder.";
else {
    $sv_errh = libxml_use_internal_errors(true); //enable user error handling
    $models_xml = simplexml_load_string($models_str);
    if (!models_xml)
        foreach (libxml_get_errors() as $error) {
            $models_err = $models_err . "\n" . $error;
        }
    libxml_clear_errors();
    libxml_use_internal_errors($sv_errh);
    $models_json = json_encode($models_xml);
}
if (!$models_json || $models_json == "") $models_json = "{}";
?>
const xlmodels = <? echo $models_json; ?>;
const xlmodels_err = "<? echo $models_err; ?>";
//xlate xLights models into Pixel Strings:
function importStrings() {
    const outtype = $('#BBB48StringSubType').val(); //"DPI24Hat"
    const driver = MapPixelStringType(outtype); //"DPIPixel"
    if (xlmodels_err) { alert(xlmodels_err); return; }
    xlmodels.models.model
    .map((model, inx) => {
        const attrs = model["@attributes"];
        const name = attrs.name;
        const starch = +attrs.StartChannel.replace(/![^:]+:/, "");
        const numpx = attrs.CustomModel? attrs.CustomModel.split(/[;,]/).filter(node => node !== "").length: attrs.parm2? +attrs.parm2: 0;
        return Object.assign(model, {inx, attrs, name, starch, numpx});
    })
    .sort((lhs, rhs) => lhs.starch - rhs.starch)
    .forEach(model => {
        const ctlr = (model.ControllerConnection || {})["@attributes"] || {};
        const [port, protocol, order, maxbr] = [ctlr.Port || model.inx, ctlr.protocol || "ws2811", ctlr.colorOrder || "RGB", ctlr.brightness || 100];
//	console.log("import model", {outtype, driver, protocol, name: model.name, starch: model.starch, numpx: model.numpx, port, order, maxbr});
        for (let substr = 0;; ++substr) { //populate blank or create new string
            const rid = rowid(driver, 0, port - 1, substr);
            let row = $('#' + rid);
            if (!row.length && substr) { //create new substring
                addVirtualString($('#' + rowid(driver, 0, port - 1, substr - 1)).find(".vsPixelCount"));
                row = $('#' + rid);
            }
            if (!row.length) throw "can't find row " + rid;
            if (+row.find(".vsPixelCount").val() || 0) continue; //skip populated strings
            setRowData(row,
                   protocol, //row.find('.vsProtocol').val(),
                   model.name,
                   model.starch,
                   model.numpx,
                   1, //row.find('.vsGroupCount').val(),
                   "0", //"Forward", //row.find('.vsReverse').val(),
                   order, //row.find('.vsColorOrder').val(),
                   0, //row.find('.vsStartNulls').val(),
                   0, //row.find('.vsEndNulls').val(),
                   0, //row.find('.vsZigZag').val(),
                   maxbr.toString(),
                   1.0); //row.find('.vsGamma').val());
//            const paranoid = row.find('.vsPixelCount').val();
//            if (paranoid != model.numpx) throw "update pixelCount failed: " + typeof(paranoid) + " " + JSON.stringify(paranoid) + " " + typeof row.find(".vsPixelCount") + " " + JSON.stringify(row.find(".vsPixelCount"));
            return;
        }
    });
    $(window).trigger('resize'); //kludge: force table to repaint
}

function loadBBBOutputs() {
    var defaultData = {};
    defaultData.channelOutputs = [];
    var output = {};
    output.type = 'BBB48String';
    <?
    if (isset($capes["F8-B"])) {
        echo 'output.subType = "F8-B";';
    } else {
        echo 'output.subType = "' . $capes[0]['name'] . '";';
        if (isset($capes[0]['pinoutVersion'])) {
            echo 'output.pinoutVersion = "' . $capes[0]['pinoutVersion'] . '";';
        } else {
            echo 'output.pinoutVersion = "1.x";';
        }
        if (isset($capes[0]['driver'])) {
            echo "output.type = \"" . $capes[0]['driver'] . "\";";
        }
    }
    ?>
    defaultData.channelOutputs.push(output);

    
<? if ($settings['Platform'] == "BeagleBone Black") { ?>
    var output = {};
    output.type = 'BBBSerial';
<?
    if (isset($capes["F8-B"])) {
        echo 'output.subType = "F8-B";';
    } else {
        echo 'output.subType = "' . $capes[0]['name'] . '";';
        if (isset($capes[0]['pinoutVersion'])) {
            echo 'output.pinoutVersion = "' . $capes[0]['pinoutVersion'] . '";';
        }
    }
?>
    output.subType = 'off';
    defaultData.channelOutputs.push(output);
<? } ?>
    
    populatePixelStringOutputs(defaultData);
    $.getJSON("api/channel/output/" + PIXEL_STRING_FILE_NAME, function(data) {
                ValidateBBBStrings(data);
                populatePixelStringOutputs(data)
              });
}
function saveBBBOutputs() {
    var postData = getPixelStringOutputJSON();
    
    <? if ($settings['Platform'] == "BeagleBone Black") { ?>
    postData = addSerialOutputJSON(postData);
    <? } ?>
    
    $.post("api/channel/output/" + PIXEL_STRING_FILE_NAME, JSON.stringify(postData)).done(function(data) {
        $.jGrowl("Pixel String Output Configuration Saved",{themeState:'success'});
        SetRestartFlag(1);
    }).fail(function() {
        DialogError("Save Pixel String Outputs", "Save Failed");
    });
}


function populateCapeList() {
    var select = document.getElementById("BBB48StringSubType");
    var option;
    <?
    foreach ($capes as $x => $x_value) {
    ?>
        option = document.createElement("option");
        option.text = '<? print_r($x_value["longName"]) ?>';
        option.value = '<? print_r($x_value["name"]) ?>';
        select.appendChild(option);
    <?
    }
    ?>
}

$(document).ready(function(){
    if (currentCapeName != "" && currentCapeName != "Unknown") {
        $('.capeName').html(currentCapeName);
        $('.capeTypeLabel').html("Cape Config");
    }

    $.get('/api/gpio')
	.done(data => selected_string_details.gpio = data)
        .fail(err => DialogError("Query gpio", "Get gpio info failed: " + err));
    populateCapeList();
    loadBBBOutputs();
});

</script>

<div id='tab-BBB48String'>
    <div id='divBBB48String'>
    
        <div class="row tablePageHeader">
            <div class="col-md"><h2><span class='capeName'>String Capes</span> </h2></div>
            <div class="col-md-auto ml-lg-auto">
                <div class="form-actions">
                    
                        <input type='button' class="buttons" onClick='loadBBBOutputs();' value='Revert'>
                        <input type='button' class="buttons" onClick='cloneSelectedString();' value='Clone String'>
                        <input type='button' class="buttons" onClick='importStrings();' value='Import Strings'>
                        <input type='button' class="buttons btn-success ml-1" onClick='saveBBBOutputs();' value='Save'>
                </div>
            </div>
        </div>
        <div class="backdrop tableOptionsForm">
            <div class="row">
                <div class="col-md-auto">
                    <div class="backdrop-dark form-inline enableCheckboxWrapper">

                    <div><b>Enable <span class='capeName'>String Cape</span>:</b></div>
                                    <div><input id='BBB48String_enable' type='checkbox'></div>
                        
                    </div>
                </div>
                <div class="col-md-auto form-inline">
                    <div><b><span class='capeTypeLabel'>Cape Type</span>:</b></div>
		    <div ><select id='BBB48StringSubType' onChange='BBB48StringSubTypeChanged();'
<? if (isSet($settings['cape-info']) && isset($settings['cape-info']['capeTypeTip'])) {  ?>
title="<?= $settings['cape-info']['capeTypeTip'] ?>"
<? } ?>
                          ></select><span id='BBB48StringSubTypeSpan'> </span></div>

                </div>
                <div class="col-md-auto form-inline">
                    <div><b id='versionTag'>Version: </b></div>
                    <div><select id='BBB48StringSubTypeVersion'>
                            <option value='1.x'>1.x</option>
                            <option value='2.x'>2.x</option>
                            <option value='3.x'>3.x</option>
                        </select>
                    </div>
                </div>
                <div class="col-md-auto form-inline"
<? if ($settings['Platform'] != "BeagleBone Black") { ?>
style="display: none;"
<? } ?>
>
                    <div><b>Pixel Timing:</b></div>
                    <div colspan="3"><select id='BBB48StringPixelTiming'>
                        <option value="0">Normal (ws281x)</option>
                        <option value="1">Slow (1903)</option>
                        </select>
                    </div>
                </div>
            </div>
        </div>
            <div id='divBBB48StringData'>
                <div>
                    <span id="pixel-string-details" class="text-muted text-left d-block" style="float: left;"></span>
                    <small class="text-muted text-right pt-2 d-block">
                        Press F2 to auto set the start channel on the next row.<br>
                        <span class='capeNotes' style='display: none;'><a href='#capeNotes'>View Cape Configuration Notes</a></span>
                    </small>

                    <?if ((isSet($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported")) {  ?>
    <div class="alert alert-danger">Unsupported string cape.  Ports are limitted to 200 pixels, no virtual strings, and no DMX.</div>
    
    <? } ?>
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
                        <div class='fppTableWrapper fppTableWrapperAsTable hidden'>
                            <div class='fppTableContents serialOutputContents' role="region" aria-labelledby="BBBSerial_Output" tabindex="0">
                        <table ports='8' id='BBBSerial_Output' class="fppSelectableRowTable">
                            <thead>
                                <tr>
                                    <th width='30px'>#</th>
                                    <th width='90px'>Start<br>Channel</th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr id='BBBSerialOutputRow1'>
                                    <td>1</td>
                                    <td><input id='BBBSerialStartChannel1' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow2'>
                                    <td>2</td>
                                    <td><input id='BBBSerialStartChannel2' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow3'>
                                    <td>3</td>
                                    <td><input id='BBBSerialStartChannel3' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow4'>
                                    <td>4</td>
                                    <td><input id='BBBSerialStartChannel4' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow5'>
                                    <td>5</td>
                                    <td><input id='BBBSerialStartChannel5' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow6'>
                                    <td>6</td>
                                    <td><input id='BBBSerialStartChannel6' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow7'>
                                    <td>7</td>
                                    <td><input id='BBBSerialStartChannel7' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow8'>
                                    <td>8</td>
                                    <td><input id='BBBSerialStartChannel8' type='number' min='1'  max='<?= FPPD_MAX_CHANNELS ?>' value='1'></td>
                                </tr>
                            </tbody>
                        </table>
                        </div>
                        </div>
                    </span>
                </div>
            </div>

    </div>
</div>
<a name='capeNotes'></a>
<span class='capeNotes' style='display: none;'><b>Cape Configuration Notes:</b><br></span>
<span class='capeNotes' id='capeNotes' style='display: none;'></span>
<style>
