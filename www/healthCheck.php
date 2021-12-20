<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');
include 'common/menuHead.inc';
?>
<script>
function HealthCheckDone() {
    // fixme, maybe hide/enable a 're-run' button here
    SetButtonState('#btnStartHealthCheck','enable');
}

function StartHealthCheck() {
    SetButtonState('#btnStartHealthCheck','disable');
    $('#healthCheckOutput').html('');
    StreamURL('healthCheckHelper.php?output=php', 'healthCheckOutput', 'HealthCheckDone', '', 'GET', null, null, true, true);
}

</script>

<title><? echo $pageTitle; ?></title>
</head>
<body class="is-loading" onLoad="StartHealthCheck();">
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'help'; 
include 'menu.inc'; ?>
  <div class="mainContainer">
    <h2 class="title d-none d-sm-block ">FPP Health Check</h2>
    <div class="pageContent">
        <div id="healthCheck" class="">

<?php
if (isset($settings["LastBlock"]) && $settings["LastBlock"] > 1000000 && $settings["LastBlock"] < 7400000) {
?>
<div id='upgradeFlag' class="alert alert-danger" role="alert">
     SD card has unused space.  Go to
     <a href="settings.php#settings-storage">Storage Settings</a> to expand the
     file system or create a new storage partition.
</div>

<?php
}
?>
        <div id="warningsRow" class="alert alert-danger"><div id="warningsTd"><div id="warningsDiv"></div></div></div>

        <button id='btnStartHealthCheck' class='buttons wideButton disabled' onClick='StartHealthCheck();'>Restart Health Check</button>

        <div name='shouldUseCSSInstead' class='container-fluid' style='width: 90%; height: 90%;' disabled id='healthCheckOutput'></div>
    </div>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
