<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');
include 'common/menuHead.inc';
?>
<script>
function skipSetup() {
    Put('api/settings/initialSetup', false, '1');
    location.href='index.php';
}

function initialSetupChanged() {
    Put('api/settings/initialSetup', true, '1');
}
</script>

<title><? echo $pageTitle; ?></title>
</head>
<body class="is-loading">
<div id="bodyWrapper">
<?php
include 'menu.inc'; ?>
  <div class="mainContainer">
    <h2 class="title d-none d-sm-block ">FPP Initial Setup</h2>
    <div class="pageContent">
        <div id="initialSetup" class="">

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
        <div class="row tablePageHeader">
            <div class="col-md">
                <h2>Initial Setup</h2>
            </div>
            <div class="col-md-auto ml-lg-auto">
                <div class="form-actions">
                    <input type='button' class='buttons' value='Skip Initial Setup' onClick='skipSetup();'>
                    <input type='button' class='buttons btn-success' value='Finish Setup' onClick='skipSetup();'>
                </div>
            </div>
        </div>
        <hr>
        <div class='container-fluid'>

<?

$extraData = "<div class='form-actions'>" .
    "<input type='button' class='buttons' value='Preview Statistics' onClick='PreviewStatistics();'> " .
    "<input type='button' class='buttons' value='Lookup Time Zone' onClick='GetTimeZone();'> " .
    "<input type='button' class='buttons' value='Lookup Location' onClick='GetGeoLocation();'> " .
    "<input type='button' class='buttons' value='Show On Map' onClick='ViewLatLon();'> " .
    "</div>";

PrintSettingGroup('initialSetup', $extraData, '', 1, '', 'initialSetupChanged',false);
?>

        </div>
    </div>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
