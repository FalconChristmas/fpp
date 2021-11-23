<!DOCTYPE html>
<html>
<head>
<?
require_once('config.php');
require_once('common.php');
include('common/menuHead.inc');

?>
<link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css">
<link rel="stylesheet" type="text/css" href="jquery/colpick/css/colpick.css">
<link rel="stylesheet" type="text/css" href="css/jquery.colpick.css">
<script type="text/javascript" src="js/jquery.timepicker.min.js"></script>
<script type="text/javascript" src="jquery/colpick/js/colpick.js"></script>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title><?= $pageTitle; ?></title>
</head>

<?php
$pages = array(
    array("name" => "playback", "title" => "Playback", ui => 0 ),
    array("name" => "av", "title" => "Audio/Video", ui => 0 ),
    array("name" => "time", "title" => "Time", ui => 0 ),
    array("name" => "ui", "title" => "UI", ui => 0 ),
    array("name" => "email", "title" => "Email", ui => 0 ),
    array("name" => "mqtt", "title" => "MQTT", ui => 0 ),
    array("name" => "output", "title" => "Input/Output", ui => 1 ),
    array("name" => "logs", "title" => "Logging", ui => 1 ),
    array("name" => "storage", "title" => "Storage", ui => 1 ),
    array("name" => "system", "title" => "System", ui => 0 ),
    array("name" => "developer", "title" => "Developer", ui => 1 )
);
?>

<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
  <div class="mainContainer">
<h1 class="title">FPP Settings</h1>
    <div class="pageContent">

            <div class='fppTabs'>
                <div id="settingsManager">
                <nav>
                    <div class="nav nav-pills" id="nav-tab" role="tablist">
                    <?php
                        foreach ($pages as $page) {
                            if ($page[ui] <= $uiLevel) {
                    ?>
                            <a class="nav-item nav-link" id="settings-<?= $page["name"]; ?>-tab"
                                data-toggle="tab"
                                href="#settings-<?= $page["name"]; ?>"
                                data-option="<?= $page["name"]; ?>"
                                role="tab"
                                aria-controls="settings-<?= $page["name"]; ?>"
                                aria-selected="true"><?= $page["title"]; ?></a>
                            <? } ?>
                    <? } ?>
                    </div>
                </nav>

                <div class="tab-content" id="nav-tabContent">
                <?php
                    foreach ($pages as $page) {
                ?>
                    <div class="tab-pane fade show <?= ($page['name'] == 'playback') ? 'active' : '';?>" id="settings-<?= $page['name']; ?>" role="tabpanel" aria-labelledby="settings-<?= $page['name']; ?>-tab">
                    <?php require_once("settings-" . $page['name'] . ".php"); ?>
                    </div>
                <? } ?>
                </div>

                <table>
        <? if ($uiLevel >= 1) { ?>
                    <tr><th align='right'><i class='fas fa-fw fa-graduation-cap ui-level-1'></i></th><th align='left'>- Advanced Level Setting</th></tr>
        <? } ?>
        <? if ($uiLevel >= 2) { ?>
                    <tr><th align='right'><i class='fas fa-fw fa-flask ui-level-2'></i></th><th align='left'>- Experimental Level Setting</th></tr>
        <? } ?>
        <? if ($uiLevel >= 3) { ?>
                    <tr><th align='right'><i class='fas fa-fw fa-code ui-level-3'></i></th><th align='left'>- Developer Level Setting</th></tr>
        <? } ?>
                </table>
            </div>
    </div>


</div>

<?php	include 'common/footer.inc'; ?>

</div>
<script>
// Enable link to tab (e.g. settings.php#settings-mqtt )
var hash = location.hash.replace(/^#/, '');
if (hash) {
    $('.nav a[href="#' + hash + '"]').tab('show');
}
// Changes hash in url bar for easy copy/paste
$('.nav a.nav-item').on('shown.bs.tab', function (e) {
    history.pushState(null, null, e.target.hash);
});
</script>
</body>
</html>
