
<script type="text/javascript">


var KNOWN_CAPES = {
<?
    function sortByLongName($a, $b) {
        return strcmp($a['longName'], $b['longName']);
    }
    
    function readCapes($cd, $capes) {
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

                        if (empty($currentCape) || (isset($json['capes']) && in_array($currentCape, $json['capes']))) {
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


function GetBBB48StringCapeFileName() {
    var mainType = $('#BBB48StringSubType').val();
    if (!mainType || mainType == "") {
        var first = Object.keys(KNOWN_CAPES)[0];
        $('#BBB48StringSubType').val(KNOWN_CAPES[first].name);
        mainType = KNOWN_CAPES[first].name;
    }
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

function HasSerial(subType) {
    var subType = GetBBB48StringCapeFileName();
    var val = KNOWN_CAPES[subType];
    return val["numSerial"] > 0;
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
