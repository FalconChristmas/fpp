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
#PixelString {
  text-align: left;
  position: relative; /* required for th sticky to work */
}
#PixelString thead th {
    font-weight: bold;
    background: #fff; /* prevent add/delete circular buttons from showing through */
    position: sticky;
    top: 0;
    z-index: 20; /* draw table header on top of add/delete buttons */
    padding: 8px 0 0 5px;
    border-bottom: 2px solid #d5d7da
}

<?
if ($settings['Platform'] == "BeagleBone Black") {
    //  BBB only supports ws2811 at this point
    ?>
    #PixelString tr > th:nth-of-type(2),
    #PixelString tr > td:nth-of-type(2) {
        display: none;
    }
    <?
}
if ((isset($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported")) {
    // don't support virtual strings
    ?>
    #PixelString tr > th:nth-of-type(3),
    #PixelString tr > td:nth-of-type(3) {
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
function sortByLongName($a, $b)
{
    return strcmp($a['longName'], $b['longName']);
}

function readCapes($cd, $capes)
{
    global $settings;
    if (is_dir($cd)) {
        $files = scandir($cd);
        foreach ($files as $file) {
            $string = "";
            if (substr($file, 0, 1) == '.') {
                $string = "";
            } else {
                $string = file_get_contents($cd . $file);
//            echo "/* file len " . strlen($string) . "*/\n";
                //            echo "/* ends with '" . substr($string, -4) . "' */\n";
            }

            if ($string != "") {
                $json = json_decode($string, true);
                if ($json['numSerial'] != 0 && isset($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported") {
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
    }
    return $capes;
}

$virtualEEPROMDir = '';
if ($settings['Platform'] == "Raspberry Pi") {
    $virtualEEPROMDir = $fppDir . '/capes/pi';
} else if ($settings['Platform'] == "BeagleBone Black") {
    $virtualEEPROMDir = $fppDir . '/capes/bbb';
    if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
        $virtualEEPROMDir = $fppDir . '/capes/pb';
    }
} else {
    $virtualEEPROMDir = $fppDir . '/capes/virtual';
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

function isLicensedDriver(driver) {
    if ((driver == 'BBB48String') || (driver == 'DPIPixels') || (driver == 'BBShiftString'))
        return true;

    return false;
}

function showVirtualEEPROMSelect() {
    $('.capeTypeRow').hide();
    $('.capeEEPROMRow').show();
}
function downloadEEPROM() {
    window.location.href = "cape-info.php";
}
function cancelVirtualEEPROMSelect() {
    reloadPage();
}


function RemoveVirtualEEPROM() {
    DeleteFile("config", null, "cape-eeprom.bin", true);
    DeleteFile("config", null, "co-pixelStrings.json", true);
    DeleteFile("config", null, "co-bbbStrings.json", true);
    DeleteFile("tmp", null, "cape-info.json", true);
    SetRebootFlag();
    reloadPage();
}

function UpgradeDone() {
    EnableModalDialogCloseButton("InstallVirtualEEPROM");
    $("#InstallVirtualEEPROMCloseButton").prop("disabled", false);
}
function InstallFirmwareDone() {
    var txt = $('#InstallVirtualEEPROMText').val();
    if (txt.includes("Cape does not match new firmware")) {
        var arrayOfLines = txt.match(/[^\r\n]+/g);
        var msg = "Are you sure you want to install the virtual firmware for cape:\n" + arrayOfLines[2] + "\n\nWith the virtual firmware for: \n" + arrayOfLines[3] + "\n";
        if (confirm(msg)) {
            var filename = $('#virtualEEPROM').val();
            $('#upgradeText').html('');
            StreamURL('upgradeCapeFirmware.php?force=true&filename=' + filename, 'InstallVirtualEEPROMText', 'UpgradeDone', 'UpgradeDone', 'GET', null, false, false);
        } else {
            reloadPage();
        }
    }
    UpgradeDone();
}

function InstallFirmware() {
    var filename = $('#virtualEEPROM').val();

    if (filename == '') {
        alert('You must select a Virtual EEPROM from the list.');
        return;
    }

    DisplayProgressDialog("InstallVirtualEEPROM", "Install Cape Firmware");
    StreamURL('upgradeCapeFirmware.php?filename=' + filename, 'InstallVirtualEEPROMText', 'InstallFirmwareDone', 'InstallFirmwareDone', 'GET', null, false, false);
}

function MapPixelStringType(type) {
    var subType = GetPixelStringCapeFileNameForSubType(type);
    if (KNOWN_CAPES[subType] && KNOWN_CAPES[subType].driver) {
        return KNOWN_CAPES[subType].driver;
    }
    return "BBB48String";
}
function MapPixelStringSubType(type) {
    return type;
}
function MapPixelStringSubTypeVersion(version) {
    return $('#PixelStringSubTypeVersion').val();
}
function GetPixelStringTiming() {
    return $('#PixelStringPixelTiming').val();
}

function SetPixelTestPattern() {
    var val = $("#PixelTestPatternType").val();
    if (val != "0") {
        var data = '{"command":"Test Start","multisyncCommand":false,"multisyncHosts":"","args":["2000","Output Specific","--ALL--","' + val + '"]}';
        $.post("api/command", data
	    ).done(function(data) {
	    }).fail(function() {
	    });
    } else {
        var data = '{"command":"Test Stop","multisyncCommand":false,"multisyncHosts":"","args":[]}';
        $.post("api/command", data
	    ).done(function(data) {
	    }).fail(function() {
	    });
    }
}

var maxVirtualStringsPerOutput = 100;
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
    <?if ($settings['Platform'] == "BeagleBone Black") {?>
    var isChannelBased = true;
    <?} else {?>
        var outtype = $('#PixelStringSubType').val();
        var driver = MapPixelStringType(outtype);
        var isChannelBased = (driver == 'DPIPixels') || (driver == 'RPIWS281X');
    <?}?>

    result += "<td>";
    result += "<select class='vsColorOrder' onChange='updateItemEndChannel(this);'>";
    result += pixelOutputTableInputOrderOption('RGB', colorOrder);
    result += pixelOutputTableInputOrderOption('RBG', colorOrder);
    result += pixelOutputTableInputOrderOption('GBR', colorOrder);
    result += pixelOutputTableInputOrderOption('GRB', colorOrder);
    result += pixelOutputTableInputOrderOption('BGR', colorOrder);
    result += pixelOutputTableInputOrderOption('BRG', colorOrder);

    if (isChannelBased) {
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
    }
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

    setupBankLimits();
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
        result += "<td>&nbsp;<span style='display: none;' data-bs-html='true' class='vsPortLabel'>" + hwLabel + "" + portPfx + ")</span></td>";
        result += "<td><input type='hidden' class='vsProtocol' value='" + protocol + "'</td>";
        result += "<td><button ";
        result += "class='circularButton circularButton-sm circularVirtualStringButton circularDeleteButton' onClick='removeVirtualString(this);'></button></td>";
    }
    else
    {
        result += "<td class='vsPortLabel' align='center' data-bs-html='true' title=''>" + hwLabel + "" + portPfx + ")</td>";
        result += "<td>" + pixelOutputProtocolSelect(protocols, protocol) + "</td>";
        result += "<td ><button ";
        result += "class='circularButton circularButton-sm circularButton-visible circularVirtualStringButton circularAddButton' onClick='addVirtualString(this);'></button></td>";
    }

    result += "<td><input type='text' class='vsDescription' size='25' maxlength='60' value='" + description + "'></td>";
    result += "<td><input type='number' class='vsStartChannel' data-bs-html='true' size='7' value='" + startChannel + "' min='1' max='<?echo FPPD_MAX_CHANNELS; ?>' onkeypress='preventNonNumericalInput(event)' onChange='updateItemEndChannel(this); sanityCheckOutputs();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
    result += "<td><input type='number' class='vsPixelCount' size='4' min='0' max='1600' onkeypress='preventNonNumericalInput(event)' value='" + pixelCount + "' onChange='updateItemEndChannel(this); sanityCheckOutputs();' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
    result += "<td><input type='number' class='vsGroupCount' size='3' value='" + groupCount + "' min='1' max='1000' onkeypress='preventNonNumericalInput(event)' onChange='updateItemEndChannel(this); sanityCheckOutputs();'></td>";
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

        while ((nextRow.find('.vsStartChannel').length == 0) && (nextRow.next('tr').length)) {
            nextRow = nextRow.next('tr');
        }
        if (!nextRow.is(":visible")) {
            nextRow = nextRow.next('tr');
        }

        nextRow.find('.vsStartChannel').val(nextStart);
        nextRow.addClass('selectedEntry');
        updateRowEndChannel(nextRow);

        selectedPixelStringRowId = nextRow.attr('id');

        sanityCheckOutputs();
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
//    selected_string_details(row);
}

function updateItemEndChannel(item) {
    var row = $(item).parent().parent();

    updateRowEndChannel(row);
}

//max pixel current:
//use current instead of power so 5V vs 12V doesn't matter
const pxAmps = {
    ws2811: 60e-3, //max current drawn by full-on px
    null_ws2811: 2e-3, //current drawn by off or null px; TODO: verify this
//TODO: add more types as needed (APA102, LPD6803/8806, WS2801, etc)
};

//show details about currently selected pixel port:
function selected_string_details(row) { //outputs, rowid) {
    let details = "";
    const pins = (GetPixelStringPins() || [])
        .map(hdr_pin => ({hdr_pin, gpio_info: (selected_string_details.gpio || []).find(pin_info => pin_info.pin == hdr_pin)}));
    const outtype = $('#PixelStringSubType').val();
    const driver = MapPixelStringType(outtype);
    const is_dpi = driver == 'DPIPixels';
    const portid = row.attr("id").replace(/_\d+$/, "");
    row = $('#' + portid + "_0"); //start with first string on this port
    const portinx = +(portid.match(/(?:_)(\d+)$/) || [])[1]; //some rows don't have port label so get it from row id
    const protocol = row.find(".vsProtocol").val();
    const pxA = [pxAmps["null_" + protocol] || 0, pxAmps[protocol] || 0];
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
            maxA += sPixelCount * sBrightness / 100 * pxA[1];
            maxA += (sPixelCount + sNullNodes + sEndNulls) * pxA[0]; //off + null px take power too
            row = row.closest('tr').next('tr');
            if ((row.attr("id") || "").replace(/_\d+$/, "") != portid) break;
        }
        const frtime = numpx? numpx * 30e-6 + 1e-3: 0; //allow 1 msec for refresh/latency, assume hsync/vsync don't add much overhead
        const port_fps = numpx? 1 / frtime: 0;
	    const cfg_fps = !numpx? "": //no output
            (port_fps < 20)? "OVERRUN": //will cause frame overrun
            (port_fps < 40)? "as 20": "as 40"; //TODO: add other fps if supported
        details += `<b>Port ${portinx + 1} (${(gpio_name !== null)? "GPIO" + gpio_name + " on ": ""}${hdr_name || "UNKNOWN PIN!"}):</b> ${plural(numpx)} pixel${plural()}, ${(frtime * 1e3).toFixed(3).replace(".000", "")} msec refresh${cfg_fps? ` (config ${cfg_fps} fps)`: ""}, ${maxA.toFixed(1).replace(".0", "")} A max`;
    } else if (driver == 'BBShiftString') {
        details += `<b>Port ${portinx + 1}</b>`;
    } else {
        details += `<b>Port ${portinx + 1}`;
        if (hdr_name) {
            details += ` (${(gpio_name !== null)? "GPIO" + gpio_name + " on ": ""}${hdr_name || "UNKNOWN PIN!"})`;
        }
        details += '</b>';
    }
//    $("#pixel-string-details").html(details);
    return details;
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
    var curRow = row.closest("tr")[0].rowIndex - 1;
    var rowsAbove = 0;
    var rowsBelow = 0;

    // Calculate how many rows actually have string data on them
    for (r = 0; r < rowCount; r++) {
        var tRow = row.parent().find('tr').eq(r);
        if (tRow.find('.vsPixelCount').length != 0) {
            if (r < curRow)
                rowsAbove--;
            else if (r > curRow)
                rowsBelow++;
        }
    }

    var clones = prompt('How many strings to clone from selected string?', rowsBelow);

    if (clones == null || clones == "" || clones == 0)
        return;

    clones = parseInt(clones);

    if ((clones > 0) && (clones > rowsBelow)) {
        alert('Max rows to clone going down is ' + rowsBelow);
        return;
    }
    if ((clones < 0) && (clones < rowsAbove)) {
        alert('Max rows to clone going up is ' + (0-rowsAbove));
        return;
    }

    var mult = (clones < 0)? -1 : 1;

    var sDescription = row.find('.vsDescription').val() || "";
    var sStartChannel = parseInt(row.find('.vsStartChannel').val()) || 1;
    var sPixelCount = parseInt(row.find('.vsPixelCount').val()) || 0;
    var startValue = parseInt((sDescription.match(/(\d+)$/) || ['1'])[0] || '0') + 1;
    var highValue = (mult * clones) + startValue - 1;

    var pad = '0';
    if (highValue < 9)
        pad = '0';
    else if (highValue < 99)
        pad = '00';
    else if (highValue < 999)
        pad = '000';
    else
        pad = '0000';

    sDescription = sDescription.replace(/(\d+)$/, '');
    row.find('.vsDescription').val(sDescription + (pad + (startValue - 1)).slice(-pad.length));

    var actRow = curRow + mult;
    for (i = 0; i < Math.abs(clones) && (actRow >= 0) && (actRow < rowCount);)
    {
        var tRow = row.parent().find('tr').eq(actRow);
        if (tRow.find('.vsPixelCount').length != 0) {
            var max = tRow.find('.vsPixelCount').attr('max');
            var label = tRow.find('.vsPortLabel').html().replace(')','');
            if (max < sPixelCount) {
                alert('ERROR, port ' + label + ' only supports ' + max + ' pixels.');
                return;
            }

            setRowData(tRow,
                       row.find('.vsProtocol').val(),
                       sDescription + (pad + (i + startValue)).slice(-pad.length),
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
            i++;
        }

        actRow += mult;
    }

    sanityCheckOutputs();
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
                    } else if (mxId  == 3) {
                        mxId = mxCnt + 9;
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

function GetPixelStringCapeFileNameForSubType(mainType) {
    var subType = "";
    var ver = $('#PixelStringSubTypeVersion').val();
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
            $('#PixelStringSubTypeVersion').val("2.x")
        } else if (subType == "-v2.json") {
            subType = ".json";
            $('#PixelStringSubTypeVersion').val("1.x")
            ver = "1.x";
        } else {
            return type;
        }
        type = mainType + subType;
    }
    return type;
}
function IsPixelStringDriverType(type) {
    return  type == "BBShiftString" || type == "BBB48String" || type == 'spixels' || type == 'RPIWS281X' || type == 'DPIPixels';
}
function GetPixelStringCapeFileName() {
    var mainType = $('#PixelStringSubType').val();
    if (!mainType || mainType == "") {
        var first = Object.keys(KNOWN_CAPES)[0];
        if (first) {
            $('#PixelStringSubType').val(KNOWN_CAPES[first].name);
            mainType = KNOWN_CAPES[first].name;
        }
    }
    return GetPixelStringCapeFileNameForSubType(mainType);
}


function GetPixelStringRequiresVersion() {
    var mainType = $('#PixelStringSubType').val();
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

function GetPixelStringRows()
{
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val) {
        return (val["outputs"] || []).length;
    }
    return -1;
}

//get array of header pin#s indexed by port#:
//NOTE: used by non-BBB capes as well
function GetPixelStringPins()
{
    const subType = GetPixelStringCapeFileName();
    var capeInfo = KNOWN_CAPES[subType];

    if (capeInfo) {
        for (var i = 0; i < capeInfo.outputs.length; i++) {
            if (capeInfo.outputs[i].hasOwnProperty('sharedOutput')) {
                capeInfo.outputs[i] = capeInfo.outputs[capeInfo.outputs[i].sharedOutput];
            }
        }
        return capeInfo.outputs && (capeInfo.outputs || []).map(info => info.pin);
    }
    return [];
}

function GetPixelStringProtocols(p) {
    var subType = GetPixelStringCapeFileName();
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
function GetPixelStringDefaultProtocol(p) {
    var subType = GetPixelStringCapeFileName();
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
    var subType = GetPixelStringCapeFileName();
    var group = {};
    if (KNOWN_CAPES[subType]) {
        var val = KNOWN_CAPES[subType];
        if (p <= val['outputs'].length && "label" in val['outputs'][p-1])
            return val['outputs'][p-1]['label'];

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
    var subType = GetPixelStringCapeFileName();
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
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val) {
        for (instance of val["groups"]) {
            if ((s == instance["start"]) && instance.hasOwnProperty('portStart')) {
                return instance['portStart'];
            }
        }
    }
    return 0;
}
function ShouldAddBreak(subType, s) {
    s = s + 1;
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val) {
        for (instance of val["groups"]) {
            if (s == instance["start"]) {
                return true;
            }
        }
    }
    return false;
}
function IsDifferential(subType, s) {
    s = s + 1;
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val) {
        for (instance of val["groups"]) {
            if (s == instance["start"]) {
                return instance["type"] == "differential";
            }
        }
    }
    return false;
}
function SupportsSmartReceivers(subType) {
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val && val.hasOwnProperty("supportsSmartReceivers")) {
        return val["supportsSmartReceivers"];
    }
    return false;
}
function SupportsFalconV5SmartReceivers(subType) {
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val && val.hasOwnProperty("supportsFalconV5SmartReceivers")) {
        return val["supportsFalconV5SmartReceivers"];
    }
    return false;
}
function IsExpansion(subType, s) {
    s = s + 1;
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val) {
        for (instance of val["groups"]) {
            if (s == instance["start"]) {
                return instance["type"] == "expansion";
            }
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
    var subType = GetPixelStringCapeFileName();
    var val = KNOWN_CAPES[subType];
    if (val && val.hasOwnProperty('numSerial')) {
        return val["numSerial"] > 0;
    }
    return 0;
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
    var subType = $('#PixelStringSubType').val();

    var subType = GetPixelStringCapeFileName();
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
    config.device = $('#PixelStringSubType').val();
    config.outputs = [];

    if (config.subType != 'off')
        config.enabled = 1;

    if (!$('#PixelString_enable').is(":checked"))
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
function PixelStringExpansionTypeChanged(port) {
    var num = 16;
    var subType = GetPixelStringCapeFileName();
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
    var type = MapPixelStringType($('#PixelStringSubType').val());

    if (val == 0 || val == -1) {
        //droping to standard/none... need to set everything to non-smart first
        for (var x = 0; x < num; x++) {
            PixelStringDifferentialTypeChangedTo((port+x), 0, 0);
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

            str += "<select id='DifferentialType" + o + "' onChange='PixelStringDifferentialTypeChanged(" + o + ");'>";
            str += "<option value='0' selected> Standard</option>";
            if (SupportsSmartReceivers(subType)) {
                str += "<option value='1'>Smart v1</option>";
                str += "<option value='2'>Smart v2</option>";
                if (SupportsFalconV5SmartReceivers(subType)) {
                    str += "<option value='3'>Falcon v5</option>";
                }
            }
            str += "</select>";
            str += "&nbsp;<select id='DifferentialCount" + o + "' onChange='PixelStringDifferentialTypeChanged(" + o + ");' style='display: none;'>";
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
function PixelStringDifferentialTypeChanged(port) {
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
        } else if (val >= 2 && len < 6) {
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
    PixelStringDifferentialTypeChangedTo(port, val, cval);

    sanityCheckOutputs();
}
function PixelStringDifferentialTypeChangedTo(port, tp, count) {
    var protocols = GetPixelStringProtocols(port);
    var protocol = GetPixelStringDefaultProtocol(port);
    var type = MapPixelStringType($('#PixelStringSubType').val());
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
            if (tp >= 1) {
                for (var y = 0; y < 4; y++) {
                    var newLabel = GetStringHWLabel(port+y) + label + ")";
                    $('#' + type + '_Output_' + x + '_' + (port + y) + '_0 td:first').html(newLabel);
                }
            } else {
                for (var y = 0; y < 4; y++) {
		            var newLabel = GetStringHWLabel(port+y) + ")";
                    $('#' + type + '_Output_' + x + '_' + (port + y) + '_0 td:first').html(newLabel);
                }
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
                if (output.enabled) {
                    $("#tab-strings-LI").show();
                }

                $('#PixelString_enable').prop('checked', output.enabled);
                var subType = output.subType;
                $('#PixelStringSubType').val(subType);
                var version = output.pinoutVersion;
                if (version == '3.x') {
                    $('#PixelStringSubTypeVersion').val("3.x");
                } else if (version == '2.x') {
                    $('#PixelStringSubTypeVersion').val("2.x");
                } else {
                    $('#PixelStringSubTypeVersion').val("1.x");
                }
                SetupBBBSerialPorts();

                if (GetPixelStringRequiresVersion()) {
                    $('#PixelStringSubTypeVersion').show();
                    $('#versionTag').show();
                } else {
                    $('#PixelStringSubTypeVersion').hide();
                    $('#versionTag').hide();
                }
                if (type == "BBB48String") {
                    $('#BBPixelTiming').show();
                } else {
                    $('#BBPixelTiming').hide();
                }
                if ((type == 'BBB48String') || (type == 'DPIPixels') || (type == 'BBShiftString') || (type == 'RPIWS281X')) {
                    $('#PixelTestPatternDiv').show();
                } else {
                    $('#PixelTestPatternDiv').hide();
                }

                if (document.getElementById("PixelStringSubType").length == 1) {
                    $('#PixelStringSubType').hide();
                    document.getElementById("PixelStringSubTypeSpan").textContent = subType;
                    $('#PixelStringSubTypeSpan').show();
                } else {
                    $('#PixelStringSubType').show();
                    $('#PixelStringSubTypeSpan').hide();
                }


                var pixelTiming = 0;
                if ('pixelTiming' in output) {
                    pixelTiming = output.pixelTiming;
                }
                $('#PixelStringPixelTiming').val(pixelTiming);


                $('#pixelOutputs').html("");

                var outputCount = GetPixelStringRows();
                var sourceOutputCount = output.outputCount;
                if (output.outputs != null) {
                    sourceOutputCount = output.outputs.length;
                }
                if (outputCount < 0) {
                    outputCount = sourceOutputCount;
                }

                var str = "";
                str += "<div class='fppTableWrapper'>" +
                    "<div class='fppTableContents' role='region' aria-labelledby='PixelString' tabindex='0'>";
                str += "<table id='PixelString' class='fppSelectableRowTable' type='" + output.subType + "' ports='" + outputCount + "'>";
                str += pixelOutputTableHeader();
                str += "<tbody>";

                var expansions = [];
                var expansionType = 0;
                var inExpansion = false;


                for (var o = 0; o < outputCount; o++)
                {
                    var protocols = GetPixelStringProtocols(o);
                    var defProtocol = GetPixelStringDefaultProtocol(o);
                    var port = {"differentialType" : 0, "expansionType" : 0};
                    var loops = 1;
                    if (o < sourceOutputCount) {
                        port = output.outputs[o];
                    }
                    var needsABreak = ShouldAddBreak(subType, o);
                    if (needsABreak || (o == 0 && IsDifferential(subType, o)) || IsDifferentialExpansion(inExpansion, expansionType, o) || IsExpansion(subType, o)) {
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


                            str += "<select id='ExpansionType" + o + "' onChange='PixelStringExpansionTypeChanged(" + o + ");'>";
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


                            var supportSmart = SupportsSmartReceivers(subType);
                            var supportFalconv5 = supportSmart ? SupportsFalconV5SmartReceivers(subType) : false;
                            var diffType = 0;
                            if (supportSmart && port.hasOwnProperty("differentialType"))
                                diffType = port["differentialType"];

                            var diffCount = 0;
                            if (diffType > 1 && diffType < 4) {
                                diffCount = diffType;
                                diffType = 1;
                            } else if (diffType > 9) {
                                diffCount = diffType - 9;
                                diffType = 3;
                                if (!supportFalconv5) {
                                    diffType = 2;
                                }
                            } else if (diffType > 3) {
                                diffCount = diffType - 3
                                diffType = 2;
                            }
                            str += "<select id='DifferentialType" + o + "' onChange='PixelStringDifferentialTypeChanged(" + o + ");'>";
                            str += "<option value='0'" + (diffType == 0 ? " selected" : "") + ">Standard</option>";
                            if (supportSmart) {
                                str += "<option value='1'" + (diffType == 1 ? " selected" : "") + ">Smart v1</option>";
                                str += "<option value='2'" + (diffType == 2 ? " selected" : "") + ">Smart v2</option>";
                                if (supportFalconv5) {
                                    str += "<option value='3'" + (diffType == 3 ? " selected" : "") + ">Falcon v5</option>";
                                }
                            }
                            str += "</select>";
                            str += "&nbsp;<select id='DifferentialCount" + o + "' onChange='PixelStringDifferentialTypeChanged(" + o + ");'";
                            if (diffType == 0) {
                                str +=  "style=' display: none;'";
                            }
                            str += ">"
                            str += "<option value='1'" + (diffCount <= 1 ? " selected" : "") + ">1 Smart Receiver</option>";
                            str += "<option value='2'" + (diffCount == 2 ? " selected" : "") + ">2 Smart Receivers</option>";
                            str += "<option value='3'" + (diffCount == 3 ? " selected" : "") + ">3 Smart Receivers</option>";
                            if (diffType >= 2) {
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
                str += "<b>Mouse over the Port Number for additional Port details.</b><br>";
                str += "</div>";
                str += "</div>";

                $('#pixelOutputs').append(str);

                expansions.forEach(function(r) {
                                   PixelStringExpansionTypeChanged(r);
                                   });

                $('#PixelString').on('mousedown', 'tr', function(event, ui) {
                    $('#pixelOutputs table tr').removeClass('selectedEntry');
                    if ($(this).find('.vsPixelCount').length != 0) {
                        $(this).addClass('selectedEntry');
                        selectedPixelStringRowId = $(this).attr('id');
                        //selected_string_details($(this)); //output.outputs, selectedPixelStringRowId);
                    } else {
                        selectedPixelStringRowId = "NothingSelected";
                    }
                });
                setTimeout(function() {
                    $('.vsPortLabel').each(function() {
                        $(this).attr("data-bs-html", "true");
                        $(this).attr("data-bs-original-title", selected_string_details($(this).parent()));
                        $(this).tooltip();
                    });
                }, 250);

                //setTimeout(pinTableHeader, 500);

                var key = GetPixelStringCapeFileNameForSubType(subType);
                var val = KNOWN_CAPES[key];
                if (val && val.hasOwnProperty('notes')) {
                    $('.capeNotes').show();
                    $('#capeNotes').html(val.notes);
                } else {
                    $('.capeNotes').hide();
                    $('#capeNotes').html('');
                }

                setTimeout(setupPixelLimits, 100);
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
                var fn = GetPixelStringCapeFileNameForSubType(output.subType);
                if (KNOWN_CAPES[fn] == null) {
                    fn = KNOWN_CAPES[Object.keys(KNOWN_CAPES)[0]];
                    if (fn) {
                        output.subType = fn.name;
                        if (typeof fn.pinoutVersion !== 'undefined') {
                            output.pinoutVersion = fn.pinoutVersion;
                        }
                    }
                } else {
                    fn = KNOWN_CAPES[fn];
                    if (typeof fn.pinoutVersion !== 'undefined') {
                        output.pinoutVersion = fn.pinoutVersion;
                    }
                }
            } else if (type == 'BBBSerial') {
                var fn = GetPixelStringCapeFileNameForSubType(output.device);
                if (KNOWN_CAPES[fn] == null) {
                    fn = KNOWN_CAPES[Object.keys(KNOWN_CAPES)[0]];
                    if (fn) {
                        output.device = fn.name;
                        if (typeof fn.pinoutVersion !== 'undefined') {
                            output.pinoutVersion = fn.pinoutVersion;
                        }
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

<?

function getLicensedOutputs()
{
    global $settings;

    if (isset($settings['cape-info']) && isset($settings['cape-info']['verifiedKeyId'])) {
        if ($settings['cape-info']['verifiedKeyId'] != 'fp') {
            return 9999;
        } else if (isset($settings['cape-info']['signed']['licensePorts'])) {
            return intval($settings['cape-info']['signed']['licensePorts']);
        }
    }

    return 0;
}

$licensedOutputs = getLicensedOutputs();
?>

var licensedOutputs = <?=$licensedOutputs?>;
var capeLimitWarning = '';
<?
if (isset($settings['cape-info']) && ($settings['cape-info']['id'] == "Unsupported")) {
    echo "capeLimitWarning = 'Unsupported string cape.  Ports are limitted to 200 pixels, no virtual strings, and no DMX.';\n";
}
?>

function sanityCheckOutputs() {
    var ok = true;
    var outtype = $('#PixelStringSubType').val();
    var driver = MapPixelStringType(outtype);
    var rowCount = $('#pixelOutputs table tbody').find('tr').length;
    var tbody = $('#pixelOutputs table tbody');

    var outputCount = 0;
	$('#pixelOutputs table').each(function() {
		$this = $(this);
        outputCount = parseInt($this.attr('ports'));
    });

    var pixelsPerOutput = [];
    var startChannels = [];
    var endChannels = [];
    var pixelCounts = [];
    var pids = [];
    var scTitles = [];
    var pcTitles = [];
    var pcNodes = [];
    var scNodes = [];
    var pcMaxes = [];
    var labels = [];
    for (i = 0; i < outputCount; i++) {
        pixelsPerOutput[i] = 0;
    }

    var isLicensed = isLicensedDriver(driver);

    /////////////////////////////////////////////////////////////////////

    // Collect the start/end pairs and total pixel counts by pid
    for (r = 0; r < rowCount; r++) {
        var tRow = $('#pixelOutputs table tbody').find('tr').eq(r);

        startChannels[r] = 0;
        endChannels[r] = 0;
        pixelCounts[r] = 0;
        pcMaxes[r] = 0;
        scTitles[r] = "";
        pcTitles[r] = "";
        pids[r] = -1;
        labels[r] = "";
        pcNodes[r] = null;
        scNodes[r] = null;

        if (tRow.find('.vsPixelCount').length != 0) {
            var scNode = tRow.find('.vsStartChannel');
            scNodes[r] = scNode;
            scNode.removeClass("inputWarning");
            startChannels[r] = parseInt(scNode.val());

            var pcNode = tRow.find('.vsPixelCount');
            pcNodes[r] = pcNode;
            pcNode.removeClass("inputError");
            pixelCounts[r] = parseInt(pcNode.val());
            pcMaxes[r] = parseInt(pcNode.attr("max"));

            endChannels[r] = parseInt(tRow.find('.vsEndChannel').html());

            var pid = parseInt(tRow.attr('pid'));
            pixelsPerOutput[pid] += pixelCounts[r];
            pids[r] = pid;

            labels[r] = tRow.find('.vsPortLabel').html().replace(')', '');

            pcNodes[r].attr("data-bs-original-title", '');
            scNodes[r].attr("data-bs-original-title", '');
        }
    }

    /////////////////////////////////////////////////////////////////////
    // Check licensed outputs and error if too many pixels on unlicensed outputs
    if (isLicensedDriver(driver) && (licensedOutputs < outputCount)) {
        for (r = 0; r < rowCount; r++) {
            if (pids[r] != -1) {
                var pid = pids[r];
                if (((pid+1) > licensedOutputs) && (pixelsPerOutput[pid] > 50)) {
                    pcTitles[r] = 'Cape is licensed for ' + licensedOutputs + ' outputs.  Unlicensed Outputs may only use 50 pixels.';
                    pcNodes[r].addClass('inputError');
                    ok = false;
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    // Error if using banks and limits exceeded
    var subType = GetPixelStringCapeFileName();
    if (KNOWN_CAPES[subType] && KNOWN_CAPES[subType].pixelLimits) {
        for (limit of KNOWN_CAPES[subType].pixelLimits) {
            if (limit.type == 'banks') {
                for (b = 0; b < limit.banks.length; b++) {
                    var bankLimit = parseInt($('#bank' + (b+1) + 'Size').html());
                    for (i = 0; i < outputCount; i++) {

                        if (limit.banks[b].includes(i) && pixelsPerOutput[i] > bankLimit) {
                            for (r = 0; r < rowCount; r++) {
                                if (pcNodes[r] != null) {
                                    var pid = pids[r];
                                    if (pid == i) {
                                        var title = pcTitles[r];
                                        if (title != '')
                                            title += '<br>';
                                        title += 'This output is part of a Virtual String or Smart Receiver chain that exceeds the bank size';
                                        pcTitles[r] = title;
                                        ncNodes[r].addClass('inputError');
                                    }
                                }
                            }

                            ok = false;
                        }
                    }
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    // Error if pixelCount is higher than max
    for (r = 0; r < rowCount; r++) {
        if (pixelCounts[r] > pcMaxes[r]) {
            var title = pcTitles[r];
            if (title != '')
                title += '<br>';
            title += 'Output Pixel Count exceeds max';
            pcTitles[r] = title;
            pcNodes[r].addClass('inputError');
        }
    }

    // Warn on Overlapping channel ranges
    // Check for overlap
    for (i = 0; i < rowCount; i++) {
        if (startChannels[i] && endChannels[i]) {
            for (j = 0; j < rowCount; j++) {
                if ((i < j) &&
                    (pixelCounts[i] && pixelCounts[j]) &&
                    (startChannels[i] || endChannels[i] || startChannels[j] || endChannels[j]) &&
                    (((startChannels[i] >= startChannels[j]) && (startChannels[i] <= endChannels[j])) ||
                     ((endChannels[i] >= startChannels[j]) && (endChannels[i] <= endChannels[j])) ||
                     ((startChannels[j] >= startChannels[i]) && (startChannels[j] <= endChannels[i])) ||
                     ((endChannels[j] >= startChannels[i]) && (endChannels[j] <= endChannels[i])))) {

                    var iTitle = scTitles[i];
                    var jTitle = scTitles[j];

                    if (scTitles[i] != '')
                        scTitles[i] += '<br>';
                    if (scTitles[j] != '')
                        scTitles[j] += '<br>';

                    var iLabel = labels[i];
                    var jLabel = labels[j];
                    var newTitle = '';
                    if (iLabel != jLabel)
                        newTitle = 'Channels for outputs ' + iLabel + ' and ' + jLabel + ' overlap.';
                    else
                        newTitle = 'Channels for Virtual Strings on output ' + iLabel + ' overlap.';

                    scTitles[i] += newTitle;
                    scTitles[j] += newTitle;
                    scNodes[i].addClass('inputWarning');
                    scNodes[j].addClass('inputWarning');
                }
            }
        }
    }

    for (r = 0; r < rowCount; r++) {
        if (startChannels[r] && endChannels[r]) {
            pcNodes[r].tooltip();
            scNodes[r].tooltip();
            pcNodes[r].attr("data-bs-original-title", pcTitles[r]);
            scNodes[r].attr("data-bs-original-title", scTitles[r]);

        }
    }

    return ok;
}

function ToggleCapeLicenseWarningVisibility() {
    if (settings.hasOwnProperty('HideLicenseWarning') && (parseInt(settings['HideLicenseWarning']) == 1)) {
        SetSetting('HideLicenseWarning', '0', 0, 0, true);
        $('#capeWarningsSpan').hide();
        $('#capeWarningsDiv').show();
    } else {
        SetSetting('HideLicenseWarning', '1', 0, 0, true);
        $('#capeWarningsSpan').show();
        $('#capeWarningsDiv').hide();
    }
}

function displayCapeLimits() {
<?
if (!isset($settings['cape-info']) || !isset($settings['cape-info']['name'])) {
    echo "return;\n";
}
?>
    var outtype = $('#PixelStringSubType').val();
    var driver = MapPixelStringType(outtype);

    $('#capeLimits').hide();
    $('#capeWarningsSpan').hide();
    $('#capeWarningsDiv').hide();
    $('#capeWarningsMessageBottom').hide();

    if ((driver != 'BBB48String') && (driver != 'DPIPixels') && (driver != "BBShiftString"))
        return;

    if (capeLimitWarning != '') {
        $('#capeLimits').show();
        $('#capeLimits').html(capeLimitWarning);
    }

    var outputCount = 0;
	$('#pixelOutputs table').each(function() {
		$this = $(this);
        outputCount = parseInt($this.attr('ports'));

        if (licensedOutputs < outputCount) {
            var message = '';
            if (licensedOutputs == 0) {
                message = "This cape EEPROM is not signed.  Output will be limited to 50 Pixels Per Output and Smart Receiver support is disabled.  See the <a href='cape-info.php'>Cape Info</a> page for more details.";
            } else {
                message = 'This cape is licensed for ' + licensedOutputs + ' out of ' + outputCount + ' outputs. ';
                message += 'Outputs higher than ' + licensedOutputs + ' will be limited to 50 Pixels Per Output and will not support Smart Receivers.';
            }

            $('#capeWarningsMessageBottom').html(message);
            $('#capeWarningsMessageBottom').show();
            $('#capeWarningsMessage').html(message);

            if (!settings.hasOwnProperty('HideLicenseWarning') || (settings['HideLicenseWarning'] == '0')) {
                $('#capeWarningsDiv').show();
            } else {
                $('#capeWarningsSpan').show();
            }
        }
    });

}

function setupBankLimits() {
    var rowCount = $('#pixelOutputs table tbody').find('tr').length;
    var subType = GetPixelStringCapeFileName();
    if (KNOWN_CAPES[subType] && KNOWN_CAPES[subType].pixelLimits) {
        for (limit of KNOWN_CAPES[subType].pixelLimits) {
            for (b = 0; b < limit.banks.length; b++) {
                var sname = 'bank' + (b+1) + 'Size';
                var bankLimit = parseInt($('#' + sname).html());
                SetSetting(sname, bankLimit, 0, 0, true);
                for (r = 0; r < rowCount; r++) {
                    var tRow = $('#pixelOutputs table tbody').find('tr').eq(r);
                    if (tRow.find('.vsPixelCount').length != 0) {
                        var pid = parseInt(tRow.attr('pid'));
                        if (licensedOutputs > pid) {
                            if (limit.banks[b].includes(pid)) {
                                tRow.find('.vsPixelCount').attr('max', bankLimit);
                                tRow.find('.vsGroupCount').attr('max', bankLimit);
                                tRow.find('.vsZigZag').attr('max', bankLimit);
                            }
                        } else {
                                tRow.find('.vsPixelCount').attr('max', 50);
                                tRow.find('.vsGroupCount').attr('max', 50);
                                tRow.find('.vsZigZag').attr('max', 50);
                        }
                    }
                }
            }
        }
    }

    sanityCheckOutputs();
}

function setupPixelLimits() {
    var subType = GetPixelStringCapeFileName();
    var rowCount = $('#pixelOutputs table tbody').find('tr').length;
    $('#bankSliderDiv').hide();
    if (KNOWN_CAPES[subType] && KNOWN_CAPES[subType].hasOwnProperty('pixelLimits')) {
        for (limit of KNOWN_CAPES[subType].pixelLimits) {
            if (licensedOutputs && (limit.type == 'banks')) {
                var bankSizes = [ 0, 0, 0 ];
                var savedValues = false;
                for (b = 0; b < bankSizes.length; b++) {
                    if (settings.hasOwnProperty('bank' + (b+1) + 'Size')) {
                        bankSizes[b] = parseInt(settings['bank' + (b+1) + 'Size']);
                        savedValues = true;
                    }
                }
                if (!savedValues) {
                    for (r = 0; r < rowCount; r++) {
                        var tRow = $('#pixelOutputs table tbody').find('tr').eq(r);
                        if (tRow.find('.vsPixelCount').length != 0) {
                            var pid = parseInt(tRow.attr('pid'));
                            var pixels = parseInt(tRow.find('.vsPixelCount').val());
                            for (b = 0; b < limit.banks.length; b++) {
                                if ((limit.banks[b].includes(pid)) && (pixels > bankSizes[b])) {
                                    bankSizes[b] = pixels;
                                }
                            }
                        }
                    }
                }

                if (limit.banks.length == 2) {
                    $('#bank1Size').html(bankSizes[0]);
                    $('#bank2Size').html(bankSizes[1]);
                    $('#bankSlider').slider({
                        range: 'max',
                        min: 1,
                        max: limit.limit - 1,
                        value: bankSizes[0],
                        slide: function(event, ui) {
                            var bank1Limit = parseInt(ui.value);
                            var bank2Limit = limit.limit - 1 - bank1Limit;
                            $('#bank1Size').html(bank1Limit);
                            $('#bank2Size').html(bank2Limit);
                        },
                        stop: function(event, ui) {
                            setupBankLimits();
                        }
                    });
                    $('#bankSliderDiv').show();
                } else if (limit.banks.length == 3) {
                    $('#bank1Size').html(bankSizes[0]);
                    $('#bank2Size').html(bankSizes[1]);
                    $('#bank3Size').html(limit.limit - (bankSizes[0] + bankSizes[1] + 2));
                    $('.bank3Class').show();
                    $('#bankSlider').slider({
                        range: true,
                        min: 1,
                        max: limit.limit - 2,
                        values: [ bankSizes[0], (bankSizes[0] + bankSizes[1]) ],
                        slide: function(event, ui) {
                            var bank1Limit = parseInt(ui.values[0]);
                            var bank2Limit = parseInt(ui.values[1]) - bank1Limit;
                            var bank3Limit = limit.limit - 2 - bank2Limit - bank1Limit;
                            $('#bank1Size').html(bank1Limit);
                            $('#bank2Size').html(bank2Limit);
                            $('#bank3Size').html(bank3Limit);
                        },
                        stop: function(event, ui) {
                            setupBankLimits();
                        }
                    });
                    $('#bankSliderDiv').show();
                } else {
                    $('#bankSliderDiv').html('<b>ERROR: This cape is configured for ' + limit.banks.length + ' banks but the UI only supports a max of 3</b>');
                    $('#bankSliderDiv').show();
                }
                setTimeout(setupBankLimits, 500);
            }
        }
    } else {
        var outtype = $('#PixelStringSubType').val();
        var driver = MapPixelStringType(outtype);
        for (r = 0; r < rowCount; r++) {
            var tRow = $('#pixelOutputs table tbody').find('tr').eq(r);
            if (tRow.find('.vsPixelCount').length != 0) {
                var pid = parseInt(tRow.attr('pid'));
                if (!isLicensedDriver(driver) || (pid < licensedOutputs)) {
                    tRow.find('.vsPixelCount').attr('max', 1600);
                    tRow.find('.vsGroupCount').attr('max', 1600);
                    tRow.find('.vsZigZag').attr('max', 1600);
                } else {
                    tRow.find('.vsPixelCount').attr('max', 50);
                    tRow.find('.vsGroupCount').attr('max', 50);
                    tRow.find('.vsZigZag').attr('max', 50);
                }
            }
        }

        sanityCheckOutputs();
    }
}

<?if ($settings['Platform'] == "BeagleBone Black") {?>
var PIXEL_STRING_FILE_NAME = "co-bbbStrings";
<?} else {?>
var PIXEL_STRING_FILE_NAME = "co-pixelStrings";
<?}?>

function PixelStringSubTypeChanged()
{
    SetSetting('HideLicenseWarning', '0', 0, 0, true);

    if (PixelStringLoaded) {
        $.getJSON("api/channel/output/" + PIXEL_STRING_FILE_NAME, function(data) {
                  for (var i = 0; i < data.channelOutputs.length; i++) {
                    if (IsPixelStringDriverType(data.channelOutputs[i].type)) {
                        data.channelOutputs[i].type = MapPixelStringType($('#PixelStringSubType').val());
                        data.channelOutputs[i].subType = $('#PixelStringSubType').val();
                    }
                    if (data.channelOutputs[i].type == 'BBBSerial') {
                      data.channelOutputs[i].device = $('#PixelStringSubType').val();
                    }
                  }
                  ValidateBBBStrings(data);
                  populatePixelStringOutputs(data)
            });
    } else {
        var defaultData = {};
        defaultData.channelOutputs = [];
        var output = {};
        output.type = MapPixelStringType($('#PixelStringSubType').val());
        output.subType = $('#PixelStringSubType').val();
        <?
if (count($capes) > 0 && isset($capes[0]['pinoutVersion'])) {
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
    displayCapeLimits();
}

//get uploaded xLights models:
<?
define("xLights_MODELS", $mediaDirectory . "/upload/xlights_rgbeffects.xml");
$models_err = "";
clearstatcache(true); //TODO: is this needed?
$models_json = "";
if (file_exists(xLights_MODELS)) {
    $models_str = file_get_contents(xLights_MODELS);
} else {
    $models_str = "";
}
if ($models_str == "") {
    $models_err = "xlights_rgbeffect.xml not found.  Please upload it from your xLights folder.";
} else {
    $sv_errh = libxml_use_internal_errors(true); //enable user error handling
    $models_xml = simplexml_load_string($models_str);
    if (!models_xml) {
        foreach (libxml_get_errors() as $error) {
            $models_err = $models_err . "\n" . $error;
        }
    }

    libxml_clear_errors();
    libxml_use_internal_errors($sv_errh);
    $models_json = json_encode($models_xml);
}
if (!$models_json || $models_json == "") {
    $models_json = "{}";
}

?>
const xlmodels = <?echo $models_json; ?>;
const xlmodels_err = "<?echo $models_err; ?>";
//xlate xLights models into Pixel Strings:
function importStrings() {
    const outtype = $('#PixelStringSubType').val(); //"DPI24Hat"
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

function loadPixelStringOutputs() {
    var defaultData = {};
    defaultData.channelOutputs = [];
    var output = {};
    output.type = 'BBB48String';
    <?
if (isset($capes["F8-B"])) {
    echo 'output.subType = "F8-B";';
} else if (count($capes) > 0) {
    echo 'output.subType = "' . $capes[0]['name'] . '";';
    if (isset($capes[0]['pinoutVersion'])) {
        echo 'output.pinoutVersion = "' . $capes[0]['pinoutVersion'] . '";';
    } else {
        echo 'output.pinoutVersion = "1.x";';
    }
    if (isset($capes[0]['driver'])) {
        echo "output.type = \"" . $capes[0]['driver'] . "\";";
    }
} else {
    echo 'output.subType = "";';
    echo 'output.pinoutVersion = "1.x";';
}
?>
    defaultData.channelOutputs.push(output);


<?if ($settings['Platform'] == "BeagleBone Black") {?>
    var output = {};
    output.type = 'BBBSerial';
<?
    if (count($capes) == 0 || isset($capes["F8-B"])) {
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
<?}?>

    populatePixelStringOutputs(defaultData);
    $.getJSON("api/channel/output/" + PIXEL_STRING_FILE_NAME, function(data) {
                ValidateBBBStrings(data);
                populatePixelStringOutputs(data)
                displayCapeLimits();
              });
}
function savePixelStringOutputs() {
    if (!sanityCheckOutputs()) {
        alert('Errors found, please check highlighted cells for issues.  Warnings are highlighted in orange and Errors in red.');
        return;
    }

    var postData = getPixelStringOutputJSON();

    <?if ($settings['Platform'] == "BeagleBone Black") {?>
    postData = addSerialOutputJSON(postData);
    <?}?>

    $.post("api/channel/output/" + PIXEL_STRING_FILE_NAME, JSON.stringify(postData)).done(function(data) {
        $.jGrowl("Pixel String Output Configuration Saved",{themeState:'success'});
        SetRestartFlag(1);
    }).fail(function() {
        DialogError("Save Pixel String Outputs", "Save Failed");
    });
}


function populateCapeList() {
    var select = document.getElementById("PixelStringSubType");
    var option;
    <?
foreach ($capes as $x => $x_value) {
    ?>
        option = document.createElement("option");
        option.text = '<?print_r($x_value["longName"])?>';
        option.value = '<?print_r($x_value["name"])?>';
        select.appendChild(option);
    <?
}
?>
}

function pinTableHeader() {
    var zp = $.Zebra_Pin($('#PixelString thead'), {
        contained: true,
        top_spacing: zebraPinSubContentTop,
        onPin: function(scroll, $element) {
                var hrow = $('#PixelString thead.Zebra_Pin tr:first');
                var drow = $('#PixelString tbody tr:nth-child(2)');
                for (var i = 1; i <= hrow.find('th').length; i++) {
                    hrow.find('th:nth-child(' + i + ')').css('width', drow.find('td:nth-child(' + i + ')').outerWidth());
                }
            }
    });
}

$(document).ready(function(){
<?
if ((isset($settings['cape-info'])) &&
    ((in_array('all', $settings['cape-info']["provides"])) ||
        (in_array('strings', $settings['cape-info']["provides"])))) {
    ?>
    if (currentCapeName != "" && currentCapeName != "Unknown") {
        $('.capeNamePixels').html(currentCapeName);
        $('.capeTypeLabel').html("Cape Config");
    }
<?
}
?>

    $.get('/api/gpio')
	.done(data => selected_string_details.gpio = data)
        .fail(err => $.jGrowl('Error: Unable to retrieve GPIO pin info.', { themeState: 'danger' }));
    populateCapeList();
    loadPixelStringOutputs();
});

</script>

<div id='tab-PixelString'>
    <div id='divPixelString'>

        <div class="row tablePageHeader capeTypeRow">
            <div class="col-md"><h2><span class='capeNamePixels'>String Capes</span> </h2></div>
            <div class="col-md-auto ml-lg-auto">
                <div class="form-actions">

<?
if (!isset($settings['cape-info']) || !isset($settings['cape-info']['name']) || file_exists($mediaDirectory . '/config/cape-eeprom.bin')) {
    $eepromFile = "/home/fpp/media/tmp/eeprom.bin";
    if (file_exists($eepromFile)) {
        echo "<input type='button' class='buttons' onClick='downloadEEPROM();' value='Download/Install EEPROM'>\n";
    }
    if (file_exists($mediaDirectory . '/config/cape-eeprom.bin')) {
        echo "<input type='button' class='buttons' onClick='showVirtualEEPROMSelect();' value='Change Virtual EEPROM'>\n";
    } else {
        echo "<input type='button' class='buttons' onClick='showVirtualEEPROMSelect();' value='Install Virtual EEPROM'>\n";
    }
}

if (isset($settings['cape-info'])) {

    echo "<input type='button' class='buttons' onClick='loadPixelStringOutputs();' value='Revert'>\n";
    echo "<input type='button' class='buttons' onClick='cloneSelectedString();' value='Clone String'>\n";
    if (file_exists($mediaDirectory . "/upload/xlights_rgbeffects.xml")) {
        echo "<input type='button' class='buttons' onClick='importStrings();' value='Import Strings'>\n";
    }
    echo "<input type='button' class='buttons btn-success ml-1' onClick='savePixelStringOutputs();' value='Save'>\n";
}
?>
                </div>
            </div>
        </div>
        <div class="backdrop tableOptionsForm capeEEPROMRow" style='display: none;'>
            <div class="row">
                <div class="col-md-auto">
                    <div>
                        <b>Virtual EEPROM:</b>
                        <select id='virtualEEPROM'>
                            <option value=''>-- Choose a Virtual EEPROM --</option>
<?
$virtName = '';
if (file_exists('/home/fpp/media/config/cape-eeprom.bin')) {
    $edata = file_get_contents('/home/fpp/media/config/cape-eeprom.bin');
    $virtName = unpack('a26', substr($edata, 6, 26));
}

$files = scandir($virtualEEPROMDir);
foreach ($files as $file) {
    if (preg_match('/-eeprom.bin$/', $file)) {
        $base = preg_replace('/-eeprom.bin/', '', $file);

        if ($virtName != '') {
            $edata = file_get_contents($virtualEEPROMDir . '/' . $file);
            $tmpName = unpack('a26', substr($edata, 6, 26));
            if ($virtName == $tmpName) {
                $selected = 'selected';
            } else {
                $selected = '';
            }

        } else {
            $selected = ($base == 'PiHat') ? 'selected' : '';
        }
        printf("<option value='$virtualEEPROMDir/$file' %s>$base</option>\n", $selected);
    }
}
?>
                        </select>
                        &nbsp;
<?if (file_exists('/home/fpp/media/config/cape-eeprom.bin')) {?>
                        <input type='button' class='buttons' value='Remove Existing' onClick='RemoveVirtualEEPROM();'>
<?}?>
                        <input type='button' class='buttons' value='Cancel' onClick='cancelVirtualEEPROMSelect();'>
                        <input type='button' class='buttons btn-success' value='Install' onClick='InstallFirmware();'>
<?if (file_exists('/home/fpp/media/config/cape-eeprom.bin')) {?>
                        <br><br><h3>Warning, changing or removing the virtual EEPROM will clear any current string configuration information and may trigger a reboot.</h3>
<?}?>
                    </div>
                </div>
            </div>
        </div>
        <div class="backdrop tableOptionsForm capeTypeRow"
<?if (!isset($settings['cape-info'])) {
    echo " style='display: none;'";
}
?>
        >
            <div class="row">
                <div class="col-md-auto">
                    <div class="backdrop-dark form-inline enableCheckboxWrapper">

                    <div><b>Enable <span class='capeNamePixels'>String Cape</span>:</b></div>
                                    <div><input id='PixelString_enable' type='checkbox'></div>

                    </div>
                </div>
                <div class="col-md-auto form-inline">
                    <div><b><span class='capeTypeLabel'>Cape Type</span>:&nbsp;</b></div>
		    <div ><select id='PixelStringSubType' onChange='PixelStringSubTypeChanged();'
<?if (isset($settings['cape-info']) && isset($settings['cape-info']['capeTypeTip'])) {?>
title="<?=$settings['cape-info']['capeTypeTip']?>"
<?}?>
                          ></select><span id='PixelStringSubTypeSpan'> </span></div>

                </div>
                <div class="col-md-auto form-inline">
                    <div><b id='versionTag'>Version: </b></div>
                    <div><select id='PixelStringSubTypeVersion'>
                            <option value='1.x'>1.x</option>
                            <option value='2.x'>2.x</option>
                            <option value='3.x'>3.x</option>
                        </select>
                    </div>
                </div>
                <div class="col-md-auto form-inline mr-auto">
                    <div  id="BBPixelTiming">
                        <b>Pixel Timing:</b>
                        <select id='PixelStringPixelTiming'>
                            <option value="0">Normal (ws281x)</option>
                            <option value="1">Slow (1903)</option>
                        </select>
                    </div>
                </div>

                <div class="col-md-auto form-inline">
                    <div id="PixelTestPatternDiv">
                        <b>Testing:</b>
                        <select id='PixelTestPatternType' onchange='SetPixelTestPattern();'>
                        <option value='0'>Off</option>
                        <option value='1'>Port Number</option>
                        <option value='2'>Pixel Count by Port</option>
                        <option value='3'>Pixel Count by String</option>
                        <option value='4'>Red Fade</option>
                        <option value='5'>Green Fade</option>
                        <option value='6'>Blue Fade</option>
                        <option value='7'>White Fade</option>
                        </select>
                    </div>
                </div>
            </div>
        </div>
            <div id='divPixelStringData' class='capeTypeRow' <?if (!isset($settings['cape-info'])) {
    echo " style='display: none;'";
}
?>>
                <div>
                    <span id="pixel-string-details" class="text-muted text-left d-block" style="float: left;"></span>
                    <small class="text-muted text-right pt-2 d-block">
                        Press F2 to auto set the start channel on the next row.<br>
                        <span class='capeNotes' style='display: none;'><a href='#capeNotes'>View Cape Configuration Notes</a><br></span>
                        <span id='capeWarningsSpan' style='display: none;'><a href='#' onClick='ToggleCapeLicenseWarningVisibility();' class='warning-text'>View Cape Warnings</a><br></span>
                    </small>

                    <div id='capeLimits' class='alert alert-danger' style='display: none;'></div>
                    <div id='capeWarningsDiv' class='alert alert-info' style='display: none;'>
                        <input type='button' class='buttons btn-success' value='Hide' onClick="ToggleCapeLicenseWarningVisibility();" style='float: right;'>
                        <span id='capeWarningsMessage'></span>
                    </div>

                    <div id='bankSliderDiv' style='display: none;'>
                        <b>This cape config uses banks of outputs which share max pixel counts.</b>
                        <table border=0 cellpadding=1 cellspacing=1>
                            <tr><td><b>Bank 1 Size:</b></td><td id='bank1Size'>0</td><td style='width: 50px;'></td>
                                <td class='bank2Class'><b>Bank 2 Size:</b></td><td id='bank2Size' class='bank2Class'>0</td><td style='width: 50px;'></td>
                                <td class='bank3Class' style='display: none;'><b>Bank 3 Size:</b></td><td id='bank3Size' class='bank3Class' style='display: none;'>0</td></tr>
                        </table>
                        <div id='bankSlider'></div>
                    </div>

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
                                    <td><input id='BBBSerialStartChannel1' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow2'>
                                    <td>2</td>
                                    <td><input id='BBBSerialStartChannel2' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow3'>
                                    <td>3</td>
                                    <td><input id='BBBSerialStartChannel3' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow4'>
                                    <td>4</td>
                                    <td><input id='BBBSerialStartChannel4' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow5'>
                                    <td>5</td>
                                    <td><input id='BBBSerialStartChannel5' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow6'>
                                    <td>6</td>
                                    <td><input id='BBBSerialStartChannel6' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow7'>
                                    <td>7</td>
                                    <td><input id='BBBSerialStartChannel7' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
                                </tr>
                                <tr id='BBBSerialOutputRow8'>
                                    <td>8</td>
                                    <td><input id='BBBSerialStartChannel8' type='number' min='1'  max='<?=FPPD_MAX_CHANNELS?>' value='1'></td>
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
<div id='capeWarningsMessageBottom' class='capeTypeRow' style='display: none;'></div>
<a name='capeNotes'></a>
<span class='capeNotes capeTypeRow' style='display: none;'><b>Cape Configuration Notes:</b><br></span>
<span class='capeNotes capeTypeRow' id='capeNotes' style='display: none;'></span>


