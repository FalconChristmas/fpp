<html>
<head>
<script type='text/javascript' src='../js/jquery-3.4.1.min.js'></script>
<script type='text/javascript' src='../js/fpp.js'></script>
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

function testEndpoint(method, item) {
    var url = $(item).closest('.endpointRow').find('.endpoint').html();
    var fields = getFieldsFromEndpoint(url);

    for (var i = 0; i < fields.length; i++) {
        var value = $(item).closest('.endpointRow').find('.input' + fields[i]).val();
        var regex = new RegExp(':' + fields[i], 'g');
        url = url.replace(regex, value);
    }

    url = url.replace(/(\/:[^)]*)/g, '/'); // Replace any remaining optional inputs (should be none)
    url = url.replace(/\(\/\)/g, '');      // Replace any empty optional inputs and their slash
    url = url.replace(/[\(\)]/g, '');      // Replace any parenthesis left over from optional inputs

    $(item).closest('.endpointRow').find('.testDataTD').show();
    $(item).closest('.endpointRow').find('.exampleDataTD').hide();

    var testData = $(item).closest('.endpointRow').find('.testData');

    if (method == 'GET') {
        $.get(url, function(data) {
            testData.html('<b>GET /api/' + url + '</b><hr>' + getJson(data));
            testData.addClass('preResponse');
        }).fail(function() {
            alert('Error GET-ing ' + url);
        });
    } else if (method == 'POST') {
        var postData = $(item).closest('.endpointRow').find('.inputPostData').val();
        $.ajax({
            url: url,
            method: 'POST',
            async: false,
            contentType: 'application/json',
            dataType: 'json',
            data: postData,
            success: function(data) {
                testData.html('<b>POST /api/' + url + '</b><br>' + postData + '<hr>' + getJson(data));
                testData.addClass('preResponse');
            },
            error: function() {
                alert('Error POST-ing ' + url);
            }
        });
    } else if (method == 'PUT') {
        var putData = $(item).closest('.endpointRow').find('.inputPostData').val();
        $.ajax({
            url: url,
            method: 'PUT',
            async: false,
            contentType: 'application/json',
            dataType: 'json',
            data: putData,
            success: function(data) {
                testData.html('<b>PUT /api/' + url + '</b><br>' + putData + '<hr>' + getJson(data));
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
                testData.html('<b>DELETE /api/' + url + '</b><hr>' + getJson(data));
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
    var methods = $(item).closest('.endpointRow').find('.methods').html().split(',');
    $(item).closest('.endpointRow').find('.endpointName').attr('rowspan', methods.length + 1);

    var testInputs = "<table border=0 cellpadding=1><tbody style='border: 0px;'>";

    var url = '/api/' + $(item).closest('.endpointRow').find('.endpoint').html();
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
        testInputs += "<input type='button' class='buttons' value='" + methods[m] + "' onClick='testEndpoint(\"" + methods[m] + "\", this);'>&nbsp;&nbsp;";
    }

    $(item).parent().html(testInputs);
}

var endpoints = {};
function loadEndpoints() {
    $.get('/api/endpoints.json', function(data) {
        endpoints = data;
        var tables = [ 'endpoints' ];
        for (var t = 0; t < tables.length; t++) {
            var d = data[tables[t]];
            for (var i = 0; i < d.length; i++) {
                var methods = Object.keys(d[i].methods);
                var row = "<tbody class='endpointRow'>";
                var rs = methods.length + 1;
                if (d[i].endpoint == 'help')
                    rs = methods.length;

                for (var m = 0; m < methods.length; m++) {
                    row += "<tr";
                    if (m == 0) {
                        row += " class='firstMethod";
                        if (m == (methods.length - 1)) {
                            row += ' lastMethod';
                        }
                        row += "'>";

                        row += "<td class='endpointName' rowspan=" + rs + ">";

                        if (d[i].hasOwnProperty('fppd')) {
                            if (d[i].fppd)
                                row += "<font color='red'><b>*</b></font>&nbsp;";
                        }

                        row += "/api/" + d[i].endpoint;

                        row += "<span class='methods' style='display: none;'>" + methods.join(',') + "</span>";

                        row += '</td>';
                    } else if (m == (methods.length - 1)) {
                        row += " class='lastMethod'>";
                    } else {
                        row += " class='middleMethod'>";
                    }
                    row += "<td style='text-align: center;'>" + methods[m] + "</td>";
                    row += '<td>' + d[i].methods[methods[m]].desc + '</td>';

                    var hasInput = 0;
                    row += "<td class='exampleDataTD'>";
                    if (d[i].methods[methods[m]].hasOwnProperty('input')) {
                        hasInput = 1;
                        row += "<b>Input:</b><br><pre class='inputData'>";
                        row += getJson(d[i].methods[methods[m]].input);
                        row += '</pre>';
                    }

                    if (d[i].methods[methods[m]].hasOwnProperty('output')) {
                        if (hasInput)
                            row += '<hr>';
                        row += "<b>Output:</b><br><pre class='outputData'>";
                        row += getJson(d[i].methods[methods[m]].output);
                        row += '</pre>';
                    }
                    row += '</td>';

                    if (m == 0) {
                        row += "<td class='testDataTD' style='display: none;' rowspan=" + rs + "><pre class='testData'></pre></td>";
                    }

                    row += "<td style='display: none;' class='endpoint'>" + d[i].endpoint + "</td>";
                    row += '</tr>';
                }

                if (d[i].endpoint != 'help') {
                    row += "<tr><td colspan=2 style='height: 100%;'><input type='button' class='buttons smallButton' value='Test' onClick='showTestInputs(this);'></td></tr>";
                }

                row += '</tbody>';
                $('#' + tables[t]).append(row);
            }
        }
    });
}

$(document).ready(function() {
    loadEndpoints();
});

</script>
<link rel='stylesheet' href='../css/fpp.css' />
<style>

.endpointTable {
    border-collapse: collapse;
}

.endpointTable tbody {
    border-bottom: 2px solid #777777;
}

td {
	vertical-align: top;
}

.endpoint {
	white-space: nowrap;
}

html {
    background: rgb(240, 240,240);
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
    border: solid 2px;
    margin-top: 5px;
    -moz-border-radius: 6px;
    border-radius: 6px;
    -webkit-border-radius: 6px;
    min-height: 65px;
    overflow: hidden;

   -moz-user-select: -moz-auto;
   -khtml-user-select: auto;
   -webkit-user-select: auto;
   -ms-user-select: auto;
   user-select: auto;
}

</style>
</head>
<body>
<div id='bodyWrapper'>
    <h2>FPP API Endpoints</h2>
    <b>NOTE: FPPD endpoints are indicated by '<font color='red'>*</font>' and require FPPD to be running or a timeout error will occur</b><br>
    <div class='fppTableWrapper'>
        <div class='fppTableContents'>
            <table class='endpointTable' id='endpoints' border=1 cellspacing=0 cellpadding=2 width='100%'>
                <thead>
                    <tr><th>Endpoint</th><th>Method</th><th>Description</th><th>Input / Output</th></tr>
                </thead>
            </table>
        </div>
    </div>
</div>
</body>
</html>
