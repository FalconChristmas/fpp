<!DOCTYPE html>
<html>
<?php include 'common/menuHead.inc'; ?>
<?php
require_once("common.php");
    if ($_SERVER['REQUEST_METHOD'] == 'POST') {
        $newht = "RewriteEngine on\nRewriteBase /proxy/\n\n";
        
        foreach( $_POST as $host ) {
            $newht = $newht . "RewriteRule ^" . $host . "$  " . $host . "/  [R,L]\n";
            $newht = $newht . "RewriteRule ^" . $host . "/(.*)$  http://" . $host . "/$1  [P,L]\n\n";
        }
        file_put_contents("$mediaDirectory/config/proxies", $newht);
    }

    
    
    if (file_exists("$mediaDirectory/config/proxies")) {
        $hta = file("$mediaDirectory/config/proxies", FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    } else {
        $hta = array();
    }
?>
<head>
<script type="text/javascript" src="js/validate.min.js"></script>

<script language="Javascript">

function UpdateLink(row) {
    var val = document.getElementById('ipRow' + row).value;
    document.getElementById('linkRow' + row).innerHTML = "<a href='proxy/" + val + "'>" + val + "</a>";
}

function AddNewProxy() {
	var currentRows = $("#proxyTable > tbody > tr").length

	$('#proxyTable tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
            "<td>" + (currentRows + 1) + "</td>" +
			"<td><input id='ipRow" + currentRows + "' class='active' type='text' size='40' oninput='UpdateLink(" + (currentRows) + ")'></td>" +
            "<td id='linkRow" + currentRows + "'> </td>" +
			"</tr>");
}
function AddProxyForHost(host) {
	var currentRows = $("#proxyTable > tbody > tr").length

	$('#proxyTable tbody').append(
		"<tr id='row'" + currentRows + " class='fppTableRow'>" +
            "<td>" + (currentRows + 1) + "</td>" +
			"<td><input id='ipRow" + currentRows + "' class='active' type='text' size='40' oninput='UpdateLink(" + (currentRows) + ")' value='" + host + "'></td>" +
            "<td id='linkRow" + currentRows + "'><a href='proxy/" + host + "'>" + host + "</a></td>" +
			"</tr>");
}

function RenumberColumns(tableName) {
	var id = 1;
	$('#' + tableName + ' tbody tr').each(function() {
		$this = $(this);
		$this.find("td:first").html(id);
		id++;
	});
}
function DeleteSelectedProxy() {
	if (tableInfo.selected >= 0) {
		$('#proxyTable tbody tr:nth-child(' + (tableInfo.selected+1) + ')').remove();
		tableInfo.selected = -1;
		SetButtonState("#btnDelete", "disable");
        RenumberColumns("proxyTable");
	}
}

function SetProxies() {
    var formStr = "<form action='proxies.php' method='post' id='proxyForm'>";
    var currentRows = $("#proxyTable > tbody > tr").length
    for (row = 0; row < currentRows; row++) {
        var val = document.getElementById('ipRow' + row).value;
        formStr += "<input type='hidden' name='ip" + row + "' value='" + val + "'/>";
    }
    formStr += "</form>";
    var form = $(formStr);
    $('body').append(form);
    form.submit();
}

var tableInfo = {
    tableName: "proxyTable",
    selected:  -1,
    enableButtons: [ "btnDelete" ],
    disableButtons: []
};

$(document).ready(function(){
    SetupSelectableTableRow(tableInfo);
<?php
foreach ($hta as $line) {
    if (strpos($line, 'http://') !== false) {
        $parts = preg_split("/[\s]+/", $line);
        $host = preg_split("/[\/]+/", $parts[2])[1];
        echo "AddProxyForHost('" . $host . "');";
    }
}
?>
});
</script>

<title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
  <div id="proxies" class="settings">
    <fieldset>
        <legend>Proxied Hosts</legend>
This is a list of ip/hostnames for which we can reach their HTTP configuration pages via proxy through this FPP instance. <p>
For example, if this FPP instance is used as a wireless proxy to another e1.31 controller where FPP communicates to the "show network" via the WIFI adapter and to the controller via the ethernet adapater, you can put the IP address of the e1.31 controller here and access the configuration pages by proxy without having to setup complex routing tables.
        <table>
            <tr>
                <td width='70px'><input type=button value='Save' onClick='SetProxies();' class='buttons'></td>
                <td width='70px'><input type=button value='Add' onClick='AddNewProxy();' class='buttons'></td>
                <td width='40px'>&nbsp;</td>
                <td width='70px'><input type=button value='Delete' onClick='DeleteSelectedProxy();' id='btnDelete' class='disableButtons'></td>
            </tr>
        </table>
        <div class="fppTableWrapper fppTableWrapperAsTable">
            <div class='fppTableContents'>
                <table id="proxyTable">
                    <thead>
                        <tr>
                            <th>#</td>
                            <th>IP/HostName</td>
                            <th>Link</th>
                        </tr>
                    </thead>
                    <tbody>
                    </tbody>
                </table>
            </div>
        </div>
    </fieldset>
  </div>
  <?php  include 'common/footer.inc'; ?>
</div>



</body>
</html>
