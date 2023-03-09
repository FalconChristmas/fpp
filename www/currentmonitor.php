<!DOCTYPE html>
<html>
<head>
<?php
require_once 'config.php';
require_once 'common.php';
include 'common/menuHead.inc';
?>
<script>


function StartMonitoring() {
    $.get('api/fppd/ports', function(data) {
        data.forEach(function(port) {
            var html = "<b>" + port["name"] + "</b><br>";
            if (port["enabled"]) {
                html += "Enabled: <i class='fas fa-check-circle text-success' title='Port Enabled'></i><br>";
            } else if (port["status"]) {
                html += "Enabled: <i class='fas fa-times-circle text-info' title='Port Disabled'></i><br>";
            } else {
                html += "Enabled: <i class='fas fa-times-circle text-danger' title='eFuse Triggered'></i><br>";
            }
            if (port["status"]) {
                html += "Status: <i class='fas fa-check-circle text-success' title='eFuse Normal'></i><br>";
            } else {
                html += "Status: <i class='fas fa-times-circle text-danger' title='eFuse Triggered'></i><br>";
            }
            html += port["ma"] + " ma";
            $("#fppPorts tr:nth-child(" + port["row"] + ") td:nth-child(" + port["col"] + ")").html(html);
        });
    });
}

</script>

<title><?echo $pageTitle; ?></title>
</head>
<body class="is-loading" onLoad="setInterval(StartMonitoring, 1000);">
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'status';
include 'menu.inc';?>
  <div class="mainContainer">
    <h2 class="title d-none d-sm-block ">FPP Current Monitor</h2>
    <div class="pageContent">
        <div id='fppSystemsTableWrapper' class='fppTableWrapper fppTableWrapperAsTable backdrop'>
            <div class='fppTableContents' role="region" aria-labelledby="fppSystemsTable" tabindex="0">
        		<table id='fppPortsTable' cellpadding='4' width="100%">
                    <tbody id='fppPorts' width="100%">
                        <tr><td width="12%"></td><td width="12%"></td><td width="12%"></td><td width="12%"></td><td width="12%"></td><td width="12%"></td><td width="12%"></td><td width="12%"></td></tr>
                        <tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>
                        <tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>
                        <tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>
                        <tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>
                        <tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>
                    </tbody>
                </table>
            </div>
        </div>

    </div>
  </div>
<?php	include 'common/footer.inc';?>
</div>
</body>
</html>
