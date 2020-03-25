
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
    $capes = readCapes("/home/fpp/media/tmp/strings/", $capes);
    if (count($capes) == 0 || $settings["showAllOptions"] == 1) {
        $capedir = "/opt/fpp/capes/bbb/strings/";
        if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
            $capedir = "/opt/fpp/capes/pb/strings/";
        }
        $capes = readCapes($capedir, $capes);
    }

    usort($capes, 'sortByLongName');
?>
};
</script>

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
    return val["outputs"].length;
}

function ShouldAddBreak(subType, s) {
    if (s == 0) {
        return false;
    }
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

    
    if (HasSerial(subType)) {
        $('#BBBSerialOutputs').show();
    } else {
        $('#BBBSerialOutputs').hide();
    }
    
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    maxPorts = val["serial"].length;

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
		output.startChannel = parseInt($('#BBBSerialStartChannel' + i).val());
        if (output.startChannel > 0) {
            count = count + 1;
        }

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

    if (val == 0 || val == -1) {
        //droping to standard/none... need to set everything to non-smart first
        for (var x = 0; x < num; x++) {
            BBB48StringDifferentialTypeChangedTo((port+x), 0);
        }
        for (var x = 0; x < num; x++) {
            if ($('#ROW_RULER_DIFFERENTIAL_' + (port+x)).length) {
                var tr = $('#ROW_RULER_DIFFERENTIAL_' + (port+x));
                tr.remove();
            }
            if (val == -1) {
                $('#BBB48String_Output_0_' + (port+x) + '_0').hide();
            } else {
                $('#BBB48String_Output_0_' + (port+x) + '_0').show();
            }
        }
        
    } else {
        //going to differential, need to add receiver type selections
        for (var x = 0; x < num; x += 4) {
            $('#BBB48String_Output_0_' + (port+x) + '_0').show();
            $('#BBB48String_Output_0_' + (port+x+1) + '_0').show();
            $('#BBB48String_Output_0_' + (port+x+2) + '_0').show();
            $('#BBB48String_Output_0_' + (port+x+3) + '_0').show();
            var o = port + x;
            var str = "<tr id='ROW_RULER_DIFFERENTIAL_" +o + "'><td colSpan='2'><hr></td>";
            str += "<td></td>";
            str += "<td style='font-size:0.7em; text-align:left; white-space: nowrap;'>Differential Receiver: ";
            
            
            str += "<select id='DifferentialType" + o + "' onChange='BBB48StringDifferentialTypeChanged(" + o + ");'>";
            str += "<option value='0' selected> Standard</option>";
            str += "<option value='1'>1 Smart Receiver</option>";
            str += "<option value='2'>2 Smart Receivers</option>";
            str += "<option value='3'>3 Smart Receivers</option>";
            str += "</select></td><td colSpan='10'><hr></td>";
            str += "</tr>";
            $("#BBB48String_Output_0_" + o + "_0").before(str);
        }
    }
}
function BBB48StringDifferentialTypeChanged(port) {
    var dt = $('#DifferentialType' + port);
    var val = dt.val();
    BBB48StringDifferentialTypeChangedTo(port, val);
}
function BBB48StringDifferentialTypeChangedTo(port, val) {
    if (val <= 2) {
        if ($('#ROW_RULER_DIFFERENTIAL_' + port + '_1').length) {
            var tr = $('#ROW_RULER_DIFFERENTIAL_' + port + '_1');
            tr.remove();
        }
        for (var i = 0; i < 4; i++) {
            for (var j = 0; j < maxVirtualStringsPerOutput; j++) {
                var id = "BBB48String_Output_" + 2 + "_" + (port + i) + "_" + j;
                if ($('#' + id).length) {
                    var row = $('#' + id);
                    row.remove();
                }
            }
        }
    }
    if (val <= 1) {
        if ($('#ROW_RULER_DIFFERENTIAL_' + port + '_0').length) {
            var tr = $('#ROW_RULER_DIFFERENTIAL_' + port + '_0');
            tr.remove();
        }
        for (var i = 0; i < 4; i++) {
            for (var j = 0; j < maxVirtualStringsPerOutput; j++) {
                var id = "BBB48String_Output_" + 1 + "_" + (port + i) + "_" + j;
                if ($('#' + id).length) {
                    var row = $('#' + id);
                    row.remove();
                }
            }
            var id = "BBB48String_Output_0_" + (port + i) + "_0";
            if ($('#' + id).length) {
                var row = $('#' + id);
                var td = row.find('.vsPortLabel');
                td.html((port + 1 + i) + ')');
            }
        }
    }
    if (val > 1) {
        for (var i = 0; i < 4; i++) {
            var id = "BBB48String_Output_0_" + (port + i) + "_0";
            if ($('#' + id).length) {
                var row = $('#' + id);
                var td = row.find('.vsPortLabel');
                td.html((port + 1 + i) + 'A)');
            }
        }
        var tr = $('#ROW_RULER_DIFFERENTIAL_' + port + '_0');
        if (!tr.length) {
            var j = 0;
            for (var j = maxVirtualStringsPerOutput-1; j > 0; j--) {
                var id = "BBB48String_Output_0_" + (port + 3) + "_" + j;
                if ($('#' + id).length) {
                    break;
                }
            }
            var str = "<tr id='ROW_RULER_DIFFERENTIAL_" + port + "_0'><td colSpan='13'><hr></td></tr>";
            for (var x = 0; x < 4; x++) {
                str += pixelOutputTableRow("BBB48String", 'ws2811', 'ws2811', 1, (port + x), 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0", "B");
            }
            $("#BBB48String_Output_0_" + (port + 3) + "_" + j).after(str);
        }
    }
    if (val > 2) {
        var tr = $('#ROW_RULER_DIFFERENTIAL_' + port + '_1');
        if (!tr.length) {
            var j = 0;
            for (var j = maxVirtualStringsPerOutput-1; j > 0; j--) {
                var id = "BBB48String_Output_1_" + (port + 3) + "_" + j;
                if ($('#' + id).length) {
                    break;
                }
            }
            var str = "<tr id='ROW_RULER_DIFFERENTIAL_" + port + "_1'><td colSpan='13'><hr></td></tr>";
            for (var x = 0; x < 4; x++) {
                str += pixelOutputTableRow("BBB48String", 'ws2811', 'ws2811', 2, (port + x), 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0", "C");
            }
            $("#BBB48String_Output_1_" + (port + 3) + "_" + j).after(str);
        }
    }
}


function populatePixelStringOutputs(data) {
    if (data) {
        for (var i = 0; i < data.channelOutputs.length; i++) {
            var output = data.channelOutputs[i];
            var type = output.type;
            if (type == 'BBB48String') {
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

                $('#pixelOutputs').html("");
                
                var outputCount = GetBBB48StringRows();

                var str = "";
                str += "<div class='fppTableWrapper'>" +
                    "<div class='fppTableContents'>";
                str += "<table id='BBB48String' type='" + output.subType + "' ports='" + outputCount + "'>";
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
                    var port = {"differentialType" : 0, "expansionType" : 0};
                    var loops = 1;
                    if (o < sourceOutputCount) {
                        port = output.outputs[o];
                    }
                    if (ShouldAddBreak(subType, o) || (o == 0 && IsDifferential(subType, o)) || IsDifferentialExpansion(inExpansion, expansionType, o)) {
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
                            str += "</select></td><td colSpan='10'><hr></td>";
                            str += "</tr>";
                            if (expansionType == -1) {
                                expansions.push(o);
                            }
                            inExpansion = true;
                        }
                        if (IsDifferential(subType, o) || IsDifferentialExpansion(inExpansion, expansionType, o)) {
                            var diffType = port["differentialType"];
                            
                            str += "<tr id='ROW_RULER_DIFFERENTIAL_" + o + "'><td colSpan='2'><hr></td>";
                            str += "<td></td>";
                            str += "<td style='font-size:0.7em; text-align:left; white-space: nowrap;'>Differential Receiver: ";
                            
                            
                            str += "<select id='DifferentialType" + o + "' onChange='BBB48StringDifferentialTypeChanged(" + o + ");'>";
                            str += "<option value='0'" + (diffType == 0 ? " selected" : "") + ">Standard</option>";
                            str += "<option value='1'" + (diffType == 1 ? " selected" : "") + ">1 Smart Receiver</option>";
                            str += "<option value='2'" + (diffType == 2 ? " selected" : "") + ">2 Smart Receivers</option>";
                            str += "<option value='3'" + (diffType == 3 ? " selected" : "") + ">3 Smart Receivers</option>";
                            str += "</select></td><td colSpan='10'><hr></td>";
                            str += "</tr>";
                            
                            if (diffType >= 2) {
                                loops = diffType;
                            }
                        } else if (!inExpansion) {
                            str += "<tr><td colSpan='13'><hr></td></tr>";
                        }
                    }
                    if (loops > 1) {
                        for (var l = 0; l < loops; l++) {
                            var pfx = "A";
                            if (l == 1) {
                                pfx = "B";
                            }
                            if (l == 2) {
                                pfx = "C";
                            }
                            for (var sr = 0; sr < 4; sr++) {
                                var o2 = o + sr;
                                if (o2 < sourceOutputCount) {
                                    var port = output.outputs[o2];
                                    var strings = port.virtualStrings;
                                    if (l == 1) {
                                        strings = port.virtualStringsB;
                                    }
                                    if (l == 2) {
                                        strings = port.virtualStringsC;
                                    }
                                    
                                    if (strings != null && strings.length > 0) {
                                        for (var v = 0; v < strings.length; v++) {
                                            var vs = strings[v];
                                            
                                            str += pixelOutputTableRow(type, 'ws2811', 'ws2811', l, o2, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, vs.zigZag, vs.brightness, vs.gamma, pfx);
                                        }
                                    } else {
                                        str += pixelOutputTableRow(type, 'ws2811', 'ws2811', l, o2, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0", pfx);
                                    }
                                } else {
                                    str += pixelOutputTableRow(type, 'ws2811', 'ws2811', l, o2, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0", pfx);
                                }
                            }
                            if (l != (loops-1)) {
                                str += "<tr id='ROW_RULER_DIFFERENTIAL_" + o + "_" + l + "'><td colSpan='13'><hr></td></tr>";
                            }
                        }
                        o+= 3;
                    } else {
                        if (o < sourceOutputCount && output.outputs[o].virtualStrings != null) {
                            var port = output.outputs[o];
                            for (var v = 0; v < port.virtualStrings.length; v++) {
                                var vs = port.virtualStrings[v];
                            
                                str += pixelOutputTableRow(type, 'ws2811', 'ws2811', 0, o, v, vs.description, vs.startChannel + 1, vs.pixelCount, vs.groupCount, vs.reverse, vs.colorOrder, vs.nullNodes, vs.zigZag, vs.brightness, vs.gamma);
                            }
                        } else {
                            str += pixelOutputTableRow(type, 'ws2811', 'ws2811', 0, o, 0, '', 1, 0, 1, 0, 'RGB', 0, 0, 100, "1.0");
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
    if (data) {
        for (var i = 0; i < data.channelOutputs.length; i++) {
            var output = data.channelOutputs[i];
            var type = output.type;
            if (type == 'BBB48String') {
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
                  ValidateBBBStrings(data);
                  populatePixelStringOutputs(data)
            });
    } else {
        var defaultData = {};
        defaultData.channelOutputs = [];
        var output = {};
        output.type = 'BBB48String';
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
    }
    ?>
    defaultData.channelOutputs.push(output);

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
    
    populatePixelStringOutputs(defaultData);
    $.getJSON("fppjson.php?command=getChannelOutputs&file=co-bbbStrings", function(data) {
                PixelStringLoaded = true;
                ValidateBBBStrings(data);
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
    populateCapeList();
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

								</select>

							</td>
                            <td><b id='versionTag'>Version: </b></td>
                            <td><select id='BBB48StringSubTypeVersion'>
                                    <option value='1.x'>1.x</option>
                                    <option value='2.x'>2.x</option>
                                    <option value='3.x'>3.x</option>
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
<?if ((isSet($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported")) {  ?>
    <tr><td colspan='2' style='color: red;'>Unsupported string cape.  Ports are limitted to 200 pixels, no virtual strings, and no DMX.</td></tr>
    
    <? } ?>
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
                        <div class='fppTableWrapper fppTableWrapperAsTable hidden'>
                            <div class='fppTableContents serialOutputContents'>
						<table ports='8' id='BBBSerial_Output'>
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
                        </div>
                        </div>
					</span>
				</div>
			</div>
		</fieldset>
	</div>
</div>
