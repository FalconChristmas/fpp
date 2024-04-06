<?
require_once './common/mqtthelp.php';

$wrapped = 1;
if (!isset($apiDir)) {
    require_once '../config.php';
    require_once '../common.php';
    $wrapped = 0;
    $apiDir = "";
    ?>
<html lang="en">
<head>
<link rel="stylesheet" href="../css/fpp.css">
<style>
    body {
        background-color: white;
    }
</style>
<script type='text/javascript' src='../js/jquery-latest.min.js'></script>
<script type='text/javascript' src='../js/fpp.js'></script>
<?
}
?>
<script>
function getJson(json) {
    if (typeof json == 'string') {
        return json;
    }

    return syntaxHighlight(JSON.stringify(json, null, 2));
}

function getFieldsFromEndpoint(url) {
    var fields = [];
    url.replace(/:[-_a-zA-Z0-9]*/g, function(match) {
        match = match.replace(/:/, '');
        fields.push(match);
    });

    return fields;
}

function testEndpoint(method, item, url) {

    // This is the tr that holds the "Test" button as well as the one
    // that eventually get the input table and POST/GET/PUSh buttons
    var parentTr = $(item).closest('tr');
    var fields = getFieldsFromEndpoint(url);

    for (var i = 0; i < fields.length; i++) {
        var value = parentTr.find('.input' + fields[i]).val();
        var regex = new RegExp(':' + fields[i], 'g');
        url = url.replace(regex, value);
    }

    url = url.replace(/(\/:[^)]*)/g, '/'); // Replace any remaining optional inputs (should be none)
    url = url.replace(/\(\/\)/g, '');      // Replace any empty optional inputs and their slash
    url = url.replace(/[\(\)]/g, '');      // Replace any parenthesis left over from optional inputs

    var testData = parentTr.prev().find('.testDataTD');
    testData.show();
    parentTr.prev().find('.exampleDataTD').hide();

    if (method == 'GET') {
        $.get(url, function(data) {
            console.log
            testData.html('<b>GET ' + url + '</b><hr>' + getJson(data));
            testData.addClass('preResponse');
        }).fail(function() {
            alert('Error GET-ing ' + url);
        });
    } else if (method == 'POST') {
        var postData = parentTr.find('.inputPostData').val();
        $.ajax({
            url: url,
            method: 'POST',
            async: false,
            contentType: 'application/json',
            dataType: 'json',
            data: postData,
            success: function(data) {
                testData.html('<b>POST ' + url + '</b><br>' + postData + '<hr>' + getJson(data));
                testData.addClass('preResponse');
            },
            error: function() {
                alert('Error POST-ing ' + url);
            }
        });
    } else if (method == 'PUT') {
        var putData = parentTr.find('.inputPostData').val();
        $.ajax({
            url: url,
            method: 'PUT',
            async: false,
            contentType: 'application/json',
            dataType: 'json',
            data: putData,
            success: function(data) {
                testData.html('<b>PUT ' + url + '</b><br>' + putData + '<hr>' + getJson(data));
                testData.addClass('preResponse');
            },
            error: function() {
                alert('Error PUT-ing ' + url);
            }
        });
    } else if (method == 'DELETE') {
        $.ajax({
            url: url,
            method: 'DELETE',
            async: false,
            success: function(data) {
                testData.html('<b>DELETE ' + url + '</b><hr>' + getJson(data));
                testData.addClass('preResponse');
            },
            error: function() {
                alert('Error DELETE-ing ' + url);
            }
        });
    }
}

function showTestInputs(item) {
    $(item).hide();
    var methods = $(item).closest('tr').find('.methods').html().split(',');
    $(item).closest('.endpointRow').find('.endpointName').attr('rowspan', methods.length + 1);

    var testInputs = "<table border=0 cellpadding=1><tbody style='border: 0px;'>";

    var url = '/api/' + $(item).closest('tr').find('.hiddenEndpoint').html();
    var fields = [];
    url.replace(/:[-_a-zA-Z0-9]*/g, function(match) {
        match = match.replace(/:/, '');
        fields.push(match);
    });

    for (var i = 0; i < fields.length; i++) {
        testInputs += "<tr><td><b>" + fields[i] + ":</b></td><td><input type='text' class='input" + fields[i] + "' size=30 maxlength=128 /></td></tr>";
    }

    if (methods.includes('POST') || methods.includes('PUT')) {
        testInputs += "<tr><td><b>POST/PUT Data:</b></td><td><input type='text' class='inputPostData' size=30 maxlength=256 /></td></tr>";
    }

    testInputs += '</tbody></table>';

    for (var m = 0; m < methods.length; m++) {
        testInputs += "<input type='button' class='buttons' value='" + methods[m] + "' onClick='testEndpoint(\"" + methods[m] + "\", this, ";
        testInputs += "\"" + url + "\");'>&nbsp;&nbsp;";
    }

    $(item).parent().html(testInputs);
}


function defaultForArg(arg, json) {
    if (typeof arg["default"] != "undefined") {
        var def = arg["default"];
        if (arg["type"] == "bool") {
            json["args"].push(def == "true" ? "true" : "false");
        } else {
            json["args"].push(def);
        }
    } else if (arg["type"] == "bool") {
        json["args"].push("false");
    } else if (arg["type"] == "int") {
        json["args"].push("0");
    } else if (arg["type"] == "datalist") {
        json["args"].push(arg["description"]);
    } else {
        json["args"].push(arg["name"]);
    }
}

var commmands = {};
function loadCommands() {
    if (commandList == "") {
        $.ajax({
        dataType: "json",
        url: "<?echo $apiDir; ?>commands",
        async: false,
        success: function(data) {
           commandList = data;
        }
        });
    }
    $.each( commandList, function(key, val) {
           var row = "<tbody class='commandRow'><tr>";
           row += "<td>" + val["name"] + "</td><td>";
           if (typeof val["description"] != "undefined") {
                row += val["description"] ;
           }
           row += "</td><td>";
           var json = { "command": val["name"], "args": [] };
           $.each(val["args"], function(key, arg) {
                  row += arg["description"].replace(/ /g, '&nbsp;') + "&nbsp;(name:&nbsp;'" + arg["name"] + "',&nbsp;&nbsp;type:&nbsp;'" + arg["type"] + "')<br>";
                  defaultForArg(arg, json);
                  });
           row += "</td><td><pre class='inputData'>";
           row += syntaxHighlight(JSON.stringify(json, null, 2));
           row += "</pre></td></tr></tbody>";
           $('#commands').append(row);
    });
}

/*
 * Anchors are dynamiclly via ajax thus auto scrolling if anchor is in url
 * will fail.  This will workaround that problem by forcing a scroll
 * afterward dynamic content is loaded.
 */
function fixScroll() {
    // Remove the # from the hash, as different browsers may or may not include it
    var hash = location.hash.replace('#','');

    if(hash != ''){
        var elements = document.getElementsByName(hash);
        if (elements.length > 0) {
            elements[0].scrollIntoView();
        }
   }
}

$(document).ready(function() {

    loadCommands();
});

</script>
<style>

.endpointTable .command {
    border-collapse: collapse;
}

.endpointTable tbody {
    border-bottom: 2px solid #E2E2E2;
}

.commandTable tbody {
    border-bottom: 2px solid #E2E2E2;
}

td {
	vertical-align: top;
}

.endpoint .command {
	white-space: nowrap;
}


#bodyWrapper {
    width: 100%;
}

hr {
    height: 1px;
    margin: 1px;
}

pre {
    border: none;
    margin: 1px;
    padding: 1px;
    background: rgb(240, 240,240);
}

.preResponse {
    background: #ddd;
}

.fppTableWrapper {
    border: solid 2px #E2E2E2;
    margin-top: 5px;
    -moz-border-radius: 6px;
    border-radius: 6px;
    -webkit-border-radius: 6px;
    min-height: 65px;

   -moz-user-select: -moz-auto;
   -khtml-user-select: auto;
   -webkit-user-select: auto;
   -ms-user-select: auto;
   user-select: auto;
}

.fppTableWrapper td, .fppTableWrapper th {
    border-color: #E2E2E2;
    border-style:solid;
}
</style>
<?
if (!$wrapped) {
    ?>
</head>
<body>
<div id='bodyWrapper'>
<?
}
?>
  <div><h2>FPP Commands</h2>
  Some internal FPP Commands are exposed for end user interaction and can be called via the REST API or via MQTT.  This page gives a test facility and a listing of the commands available with their arguments.
  </div>
  <div>
    <b>NOTE: FPPD Commands require FPPD to be running or a timeout error will occur.</b>
</div>
<br>
<div>
    <h2>Quick Links</h2>
    <ol>
        <li><a href="#command_tester">Command Tester</a></li>
        <li><a href="#command_list">Command List</a></li>
        <li><a href="#mqtt_instructions">Mqtt Instructions</a></li>
    <ol>
</div>
<div><h2><a name="command_tester">api/Command Tester</a></h2>

<table class="endpointTable" id="endpoints" width="100%" cellspacing="0" cellpadding="2" border="1">
                <thead>
                    <tr><th>Endpoint</th><th>Method</th><th>Description</th><th>Input / Output</th></tr>
                </thead>
<tr class="endpointRow firstMethod"><td class="endpointName" rowspan="3"><a class="api-anchor" name="command--CommandName---Args-">.</a><font color="red"><b>*</b></font>&nbsp;/api/command/:CommandName(/:Args)</td>
<td class="endpointMethod">GET</td><td class="endpointDescription">Runs the given command with arguments separated by a slash /</td><td class="exampleDataTD"><b>Output:</b><br><pre class="outputData">(dependent on Command run)</pre></td><td class="testDataTD" style="display: none;" rowspan="2"><pre class="testData"></pre></td><td style="display: none;" class="endpoint">command/:CommandName(/:Args)</td></tr>

<tr class="endpointRow lastMethod"><td class="endpointMethod">POST</td><td class="endpointDescription">Runs the given command with arguments provided in POST data instead of URL</td><td class="exampleDataTD"><b>Input:</b><br><pre class="inputData">(JSON data dependent on command being run)</pre><hr><b>Output:</b><br><pre class="outputData">(dependent on Command run)</pre></td><td class="testDataTD" style="display: none;" rowspan="2"><pre class="testData"></pre></td><td style="display: none;" class="endpoint">command/:CommandName(/:Args)</td></tr>

<tr><td colspan="2" style="height: 100%;"><table cellpadding="1" border="0"><tbody style="border: 0px;"><tr><td><b>CommandName:</b></td><td><input type="text" class="inputCommandName" size="30" maxlength="128"></td></tr><tr><td><b>Args:</b></td><td><input type="text" class="inputArgs" size="30" maxlength="128"></td></tr><tr><td><b>POST/PUT Data:</b></td><td><input type="text" class="inputPostData" size="30" maxlength="256"></td></tr></tbody></table><input type="button" class="buttons" value="GET" onclick="testEndpoint('GET', this, '/api/command/:CommandName(/:Args)');">&nbsp;&nbsp;<input type="button" class="buttons" value="POST" onclick="testEndpoint('POST', this, '/api/command/:CommandName(/:Args)');">&nbsp;&nbsp;</td></tr>
</table>
</div>
<br><div class="clear"></div>
<h2><a name="command_list">FPP Commands Listing</a></h2>
    <div class='fppTableWrapper fppTableWrapperAsTable'>
        <div class='fppTableContents' role="region" aria-labelledby="commandTable" tabindex="0">
            <table class='commandTable' id='commands' border=1 cellspacing=0 cellpadding=2 width='100%'>
                <thead>
                    <tr><th>Command</th><th>Description</th><th>Arguments</th><th>Example POST</th></tr>
                </thead>
            </table>
        </div>
    </div>
    <br><div class="clear"></div>
    <h2><a name="mqtt_instructions">MQTT Instructions</a></h2>
    <?showMqttHelpTable()?>
</div>

<?
if (!$wrapped) {
    ?>
</div>
</body>
<?
}
?>
