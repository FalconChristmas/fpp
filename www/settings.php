<!DOCTYPE html>
<html>
<head>
<?
require_once('config.php');
require_once('common.php');
include('common/menuHead.inc');

?>
<link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css">
<script type="text/javascript" src="js/jquery.timepicker.min.js"></script>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title><? echo $pageTitle; ?></title>

<script type="text/javascript" src="jquery/Spin.js/spin.js"></script>
<script type="text/javascript" src="jquery/Spin.js/jquery.spin.js"></script>

<script>

function bindSettingsVisibilityListener() {
    var visProp = getHiddenProp();
    if (visProp) {
      var evtname = visProp.replace(/[H|h]idden/,'') + 'visibilitychange';
      document.addEventListener(evtname, handleSettingsVisibilityChange);
    }
}

function handleSettingsVisibilityChange() {
    if (isHidden() && statusTimeout != null) {
        clearTimeout(statusTimeout);
        statusTimeout = null;
    } else {
        UpdateCurrentTime();
    }
}

function reloadSettingsPage() {
    location.href = 'settings.php?tab=' + $('#tabs').tabs('option', 'active');
}

var hiddenChildren = {};
function UpdateChildSettingsVisibility() {
    hiddenChildren = {};
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](2); // Hide if necessary
    });
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](1); // Show if not hidden
    });
}

var statusTimeout = null;
function UpdateCurrentTime(once = false) {
    $.get('api/time', function(data) {
        $('#currentTime').html(data.time);
        if (!once)
            statusTimeout = setTimeout(UpdateCurrentTime, 1000);
    });
}

$(document).ready(function() {
    UpdateChildSettingsVisibility();
    bindSettingsVisibilityListener();
});

</script>

</head>

<?php
$tabId = "Playback";
if (isset($_GET['tab'])) {
    $tabId = $_GET['tab'];
}
?>

<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
    <div class='fppTabs'>
        <div id="settingsManager">
        <br />
            <div class='title'>FPP Settings</div>
            <div id="tabs" style='display:none'>
                <ul>
                    <li><a href='settings-playback.php'>Playback</a></li>
                    <li><a href='settings-av.php'>Audio/Video</a></li>
                    <li><a href='settings-time.php'>Time</a></li>
                    <li><a href='settings-ui.php'>UI</a></li>
                    <li><a href='settings-email.php'>Email</a></li>
                    <li><a href='settings-mqtt.php'>MQTT</a></li>
<? if ($uiLevel >= 1 || $tabId == "Output") echo "<li><a href='settings-output.php'>Output</a></li>\n"; ?>
                    <li><a href='settings-logs.php'>Logging</a></li>
<? if ($uiLevel >= 1 || $tabId == "Storage") echo "<li><a href='settings-storage.php'>Storage</a></li>"; ?>
                    <li><a href='settings-system.php'>System</a></li>
<? if ($uiLevel >= 3 || $tabId == "Developer") echo "<li><a href='settings-developer.php'>Developer</a></li>\n"; ?>
                </ul>
            </div>
        </div>

        <table>
<? if ($uiLevel >= 1) { ?>
            <tr><th align='right'>*</th><th align='left'>- Advanced Level Setting</th></tr>
<? } ?>
<? if ($uiLevel >= 2) { ?>
            <tr><th align='right'>**</th><th align='left'>- Experimental Level Setting</th></tr>
<? } ?>
<? if ($uiLevel >= 3) { ?>
            <tr><th align='right'>***</th><th align='left'>- Developer Level Setting</th></tr>
<? } ?>
        </table>
    </div>

<?php	include 'common/footer.inc'; ?>

<script>
var activeTabNumber =
<?php
    $tabIDs = Array();
    $id = 0;
    $tabIDs["Playback"] = $id++;
    $tabIDs["AV"] = $id++;
    $tabIDs["Time"] = $id++;
    $tabIDs["UI"] = $id++;
    $tabIDs["Email"] = $id++;
    $tabIDs["MQTT"] = $id++;
    if ($uiLevel >= 1 || $tabId == "Output") $tabIDs["Output"] = $id++;
    $tabIDs["Logging"] = $id++;
    if ($uiLevel >= 1 || $tabId == "Storage") $tabIDs["Storage"] = $id++;
    $tabIDs["System"] = $id++;
    if ($uiLevel >= 3 || $tabId == "Developer") $tabIDs["Developer"] = $id++;
    
    if (!array_key_exists($tabId, $tabIDs)) {
        print $tabId;
    } else {
        print $tabIDs[$tabId];
    }
?>;

var currentLoadingTab = 0;
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
        if (ui.newTab.find("a").attr("href") == 'settings-time.php') {
            UpdateCurrentTime();
        } else if (statusTimeout != null) {
            clearTimeout(statusTimeout);
            statusTimeout = null;
        }
    },
    beforeLoad: function(event, ui) {
        if ($(ui.panel).html()) {
            event.preventDefault(); // don't reload
        }
    },
    load: function(event, ui) {
        UpdateChildSettingsVisibility();
        InitializeTimeInputs();
        InitializeDateInputs();

        currentLoadingTab++;
        if (currentLoadingTab < $('#tabs').find('li').length) {
            $('#tabs').tabs('load', currentLoadingTab);
        }
    }
});

$( function() {
    SetupToolTips();
});

$('#tabs').show();

</script>

</div>
</body>
</html>
