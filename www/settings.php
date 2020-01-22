<!DOCTYPE html>
<html>
<head>
<?
require_once('config.php');
require_once('common.php');
include('common/menuHead.inc');

$sInfos = json_decode(file_get_contents('settings.json'), true);

function stt($setting) {
    global $sInfos;
    if (isset($sInfos['settings'][$setting])) {
        ToolTip($setting, $sInfos['settings'][$setting]['tip']);
    }
}
?>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title><? echo $pageTitle; ?></title>

<script type="text/javascript" src="jquery/Spin.js/spin.js"></script>
<script type="text/javascript" src="jquery/Spin.js/jquery.spin.js"></script>

<script>

function reloadSettingsPage() {
    location.href = '/settings.php?tab=' + $('#tabs').tabs('option', 'active');
}

</script>

</head>

<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<div id="fileManager">
    <br />
    <div class='title'>FPP Settings</div>
    <div id="tabs">
        <ul>
            <li><a href="#tab-playback">Playback</a></li>
            <li><a href="#tab-network">Network</a></li>
            <li><a href="#tab-time">Time</a></li>
            <li><a href="#tab-ui">UI</a></li>
            <li><a href="#tab-email">Email</a></li>
            <li><a href="#tab-proxy">Proxy</a></li>
            <li><a href="#tab-mqtt">MQTT</a></li>
            <li><a href="#tab-output">Output</a></li>
            <li><a href="#tab-logs">Logging</a></li>
            <li><a href="#tab-system">System</a></li>
<? if ($uiLevel >= 3) echo "<li><a href='#tab-developer'>Developer</a></li>\n"; ?>
        </ul>

    <div id='tab-playback'>
        <? include 'settings-playback.php'; ?>
    </div>

    <div id='tab-network'>
        <? include 'settings-network.php'; ?>
    </div>

    <div id='tab-time'>
        <? include 'settings-time.php'; ?>
    </div>

    <div id='tab-ui'>
        <? include 'settings-ui.php'; ?>
    </div>

    <div id='tab-email'>
        <? include 'settings-email.php'; ?>
    </div>

    <div id='tab-proxy'>
        <? include 'settings-proxy.php'; ?>
    </div>

    <div id='tab-mqtt'>
        <? include 'settings-mqtt.php'; ?>
    </div>

    <div id='tab-output'>
        <? include 'settings-output.php'; ?>
    </div>

    <div id='tab-logs'>
        <? include 'settings-logs.php'; ?>
    </div>

    <div id='tab-system'>
        <? include 'settings-system.php'; ?>
    </div>

<? if ($uiLevel >= 3) { ?>
    <div id='tab-developer'>
        <? include 'settings-developer.php'; ?>
    </div>
<? } ?>

</div>

<table>
<? if ($uiLevel >= 1) { ?>
<tr><th align='right'>*</th><th align='left'>- Advanced Level Setting</th>
<? } ?>
<? if ($uiLevel >= 2) { ?>
<tr><th align='right'>**</th><th align='left'>- Experimental Level Setting</th>
<? } ?>
<? if ($uiLevel >= 3) { ?>
<tr><th align='right'>***</th><th align='left'>- Developer Level Setting</th>
<? } ?>
</table>

<? if ($uiLevel >= 1) { ?>
<br>
<a href="advancedsettings.php">Advanced Settings</a>  NOTE: The Advanced Settings page is being deprecated with settings moved into appropriate tabs on this page and marked as Advanced settings.
<? } ?>

<?php	include 'common/footer.inc'; ?>

<script>
	var activeTabNumber = 
<?php
	if (isset($_GET['tab']))
		print $_GET['tab'];
	else
		print "0";
?>;

    $("#tabs").tabs( {
        cache: true,
        active: activeTabNumber,
        spinner: "",
        fx: {
            opacity: 'toggle',
            height: 'toggle'
        },
        activate: function(event, ui) {
            $('.ui-tooltip').hide();
        }
    });

$( function() {
    $(document).tooltip({
        content: function() {
            $('.ui-tooltip').hide();
            var id = $(this).attr('id');
            id = id.replace('_img', '_tip');
            //alert('here, tip: ' + $('#' + id).html());
            return $('#' + id).html();
        },
        hide: { delay: 1000 }
    });
});

</script>

</body>
</html>
