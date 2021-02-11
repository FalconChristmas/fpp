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
<title><? echo $pageTitle; ?></title>


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
    location.href = 'settings.php?tab=' + $('#settingsManagerTabs .nav-link.active').data('option');
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

<div class="container">
<h1 class="title">FPP Settings</h1>
    <div class="pageContent">
        
            <div class='fppTabs'>
                <div id="settingsManager">
                    <ul id="settingsManagerTabs" class="nav nav-pills pageContent-tabs" role="tablist">
                        <li class="nav-item">
                            <a class="nav-link" id="settings-playback-tab" data-toggle="tab" href="#settings-playback" data-option="Playback" role="tab" aria-controls="settings-playback" aria-selected="true">
                                Playback
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-av-tab" data-toggle="tab" href="#settings-av" data-option="AV" role="tab" aria-controls="settings-av" aria-selected="true">
                            Audio/Video
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-time-tab" data-toggle="tab" href="#settings-time" data-option="Time" role="tab" aria-controls="settings-time" aria-selected="true">
                            Time
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-ui-tab" data-toggle="tab" href="#settings-ui" data-option="UI" role="tab" aria-controls="settings-ui" aria-selected="true">
                                UI
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-email-tab" data-toggle="tab" href="#settings-email" data-option="Email" role="tab" aria-controls="settings-email" aria-selected="true">
                                Email
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-mqtt-tab" data-toggle="tab" href="#settings-mqtt" data-option="MQTT" role="tab" aria-controls="settings-mqtt" aria-selected="true">
                                MQTT
                            </a>
                        </li>
                        <? if ($uiLevel >= 1 || $tabId == "Output"){?>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-output-tab" data-toggle="tab" href="#settings-output" data-option="Output" role="tab" aria-controls="settings-output" aria-selected="true">
                            Input/Output
                            </a>
                        </li>
                        <? } ?>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-logs-tab" data-toggle="tab" href="#settings-logs" data-option="Logging" role="tab" aria-controls="settings-logs" aria-selected="true">
                                Logging
                            </a>
                        </li>
                        <? if ($uiLevel >= 1 || $tabId == "Storage"){?>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-storage-tab" data-toggle="tab" href="#settings-storage" data-option="Storage" role="tab" aria-controls="settings-storage" aria-selected="true">
                            Storage
                            </a>
                        </li>
                        <? } ?>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-system-tab" data-toggle="tab" href="#settings-system" data-option="System" role="tab" aria-controls="settings-system" aria-selected="true">
                            System
                            </a>
                        </li>
                        <? if ($uiLevel >= 1 || $tabId == "Storage"){?>
                        <li class="nav-item">
                            <a class="nav-link" id="settings-developer-tab" data-toggle="tab" href="#settings-developer" data-option="Developer" role="tab" aria-controls="settings-developer" aria-selected="true">
                            Developer
                            </a>
                        </li>
                        <? } ?>
                    </ul>
                    <div id="settingsManagerTabsContent" class="tab-content">
                        <div class="spinner-border spinner-danger spinner-lg" role="status">
                            <span class="sr-only">Loading...</span>
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
    </div>
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
var tabRequests=[];
var tabElements={}
$('#settingsManagerTabs .nav-link').each(function(i){
    var tabName = $(this).attr('href').slice(1);
    tabRequests.push($.ajax({
        url:tabName+".php"
    }).done(function(data) {
        
        var $tabContent = $('<div class="tab-pane fade" id="'+tabName+'" role="tabpanel" aria-labelledby="'+tabName+'-tab"/>');
        tabElements[i] = $tabContent.html(data);
    }));
});

$.when.apply( undefined, tabRequests ).then(function() {
   $('#settingsManagerTabsContent>.spinner-border').hide();
    $.each(tabElements,function(i,$tabContent){
        if(i==activeTabNumber){
            $tabContent.addClass('show active');
        }
        $('#settingsManagerTabsContent').append($tabContent);
    });
    $('#settingsManagerTabs .nav-link').eq(activeTabNumber).addClass('active');
    $('a[data-toggle="tab"]').on('shown.bs.tab', function (e) {
        if ($(this).attr("href") == 'settings-time.php') {
            UpdateCurrentTime();
        } else if (statusTimeout != null) {
            clearTimeout(statusTimeout);
            statusTimeout = null;
        }
    });
    UpdateChildSettingsVisibility();
    InitializeTimeInputs();
    InitializeDateInputs();
});
/*
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

*/
$( function() {
    SetupToolTips();
});
/*
$('#tabs').show();
*/
</script>

</div>
</body>
</html>
