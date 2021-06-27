<!DOCTYPE html>
<html>
<?php 
require_once('config.php');
include 'common/menuHead.inc'; 
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
    var proxyLink = "";
    if (! (isValidHostname(val)  || isValidIpAddress(val) ) ) {
        proxyLink = "Must be either valid IP address or Valid Hostname"
    } else {
        proxyLink = "<a href='proxy/" + val + "'>" + val + "</a>";
    }
    document.getElementById('linkRow' + row).innerHTML = proxyLink;
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
    var row=0;

    $("input[id^=ipRow]").each(function() {
        var val = $(this).val();
        if (! (isValidHostname(val)  || isValidIpAddress(val) ) ) {
            var msg = "Skipping valid link: " + val;
            $.jGrowl(msg,{themeState:'danger'});
        } else {
            formStr += "<input type='hidden' name='ip" + row + "' value='" + val + "'/>";
            ++row;
        }
    });

    formStr += "</form>";
    console.log(formStr);
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
  <?php 
  $activeParentMenuItem = 'status';
  include 'menu.inc'; ?>
  <div class="mainContainer">
    <div class="title">Proxied Hosts</div>
    <div class="pageContent">
        
          <div id="proxies" class="settings">

                <div class="row tablePageHeader">
					<div class="col-md">
						<h2>Proxied Hosts</h2>
					</div>
					<div class="col-md-auto ml-lg-auto">
						<div class="form-actions">
			
								<input type=button value='Delete' onClick='DeleteSelectedProxy();' data-btn-enabled-class="btn-outline-danger" id='btnDelete' class='disableButtons'>
								<button type=button value='Add' onClick='AddNewProxy();' class='buttons btn-outline-success'><i class="fas fa-plus"></i> Add</button>
								<input type=button value='Save' onClick='SetProxies();' class='buttons btn-success ml-1'>

						</div>
					</div>
				</div>
				<hr>
  
                <div class="fppTableWrapper fppTableWrapperAsTable">
                    <div class='fppTableContents' role="region" aria-labelledby="proxyTable" tabindex="0">
                        <table id="proxyTable" class="fppSelectableRowTable">
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
                <div class="backdrop">
                        <b>Notes:</b>    
                        <p>This is a list of ip/hostnames for which we can reach their HTTP configuration pages via proxy through this FPP instance. <p>
                         <p>For example, if this FPP instance is used as a wireless proxy to another e1.31 controller where FPP communicates to the "show network" via the WIFI adapter and to the controller via the ethernet adapater, you can put the IP address of the e1.31 controller here and access the configuration pages by proxy without having to setup complex routing tables.
                        </p>
                </div> 
          </div>
          
    </div>
</div>
  <?php  include 'common/footer.inc'; ?>
</div>



</body>
</html>
