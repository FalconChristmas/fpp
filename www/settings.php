<!DOCTYPE html>
<html lang="en">

<head>
    <?
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';
    include 'common/menuHead.inc';

    ?>
    <link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css?ref=<?= filemtime('css/jquery.timepicker.css'); ?>">
    <link rel="stylesheet" type="text/css" href="jquery/colpick/css/colpick.css?ref=<?= filemtime('jquery/colpick/css/colpick.css'); ?>">
    <link rel="stylesheet" type="text/css" href="css/jquery.colpick.css?ref=<?= filemtime('css/jquery.colpick.css'); ?>">
    <script type="text/javascript" src="js/jquery.timepicker.js?ref=<?= filemtime('js/jquery.timepicker.js'); ?>"></script>
    <script type="text/javascript" src="jquery/colpick/js/colpick.js?ref=<?= filemtime('jquery/colpick/js/colpick.js'); ?>"></script>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <title><? echo $pageTitle; ?></title>


    <script>

        function bindSettingsVisibilityListener() {
            var visProp = getHiddenProp();
            if (visProp) {
                var evtname = visProp.replace(/[H|h]idden/, '') + 'visibilitychange';
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
            location.reload();
        }

        var hiddenChildren = {};
        function UpdateChildSettingsVisibility() {
            hiddenChildren = {};
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                window[fn](2); // Hide if necessary
            });
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                window[fn](1); // Show if not hidden
            });
        }

        var statusTimeout = null;
        function UpdateCurrentTime(once = false) {
            if (statusTimeout != null) {
                clearTimeout(statusTimeout);
                statusTimeout = null;
            }

            $.get('api/time', function (data) {
                $('#currentTime').html(data.time);
                if (!once)
                    statusTimeout = setTimeout(UpdateCurrentTime, 1000);
            });
        }

        $(document).ready(function () {
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
$storageUILevel = 1;
if (isset($settings["UnpartitionedSpace"]) && $settings['UnpartitionedSpace'] > 0) {
    $storageUILevel = 0;
}
if ($storageUILevel > 0 && $settings['Platform'] != "MacOS" && $settings['Platform'] != "Docker") {
    exec('findmnt -n -o SOURCE / | colrm 1 5', $rootDevOutput, $return_val);
    $rootDev = isset($rootDevOutput[0]) ? trim($rootDevOutput[0]) : "";
    unset($rootDevOutput);
    $flashTargets = array();
    if (preg_match('/^mmcblk0p/', $rootDev)) {
        $flashTargets = array("mmcblk1", "nvme0n1", "sda");
    } else if (preg_match('/^mmcblk1p/', $rootDev) && strpos($settings['SubPlatform'], 'PocketBeagle2') !== false) {
        $flashTargets = array("mmcblk0");
    }
    foreach ($flashTargets as $target) {
        if (file_exists("/dev/" . $target)) {
            exec("lsblk -b -d -n -o SIZE /dev/" . $target, $sizeOutput, $return_val);
            $sizeGB = (isset($sizeOutput[0]) ? intval($sizeOutput[0]) : 0) / 1024 / 1024 / 1024;
            unset($sizeOutput);
            if ($sizeGB > 12) {
                $storageUILevel = 0;
                break;
            }
        }
    }
}
$tabIDs = array();
$id = 0;
?>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">FPP Settings</h1>
            <div class="pageContent">
                <div class='fppTabs'>
                    <div id="settingsManager">
                        <ul id="settingsManagerTabs" class="nav nav-pills pageContent-tabs" role="tablist">
                            <li class="nav-item">
                                <? $tabIDs["Playback"] = $id++; ?>
                                <a class="nav-link" id="settings-playback-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-playback" href="#settings-playback" data-option="Playback"
                                    role="tab" aria-controls="settings-playback" aria-selected="true">
                                    Playback
                                </a>
                            </li>
                            <li class="nav-item">
                                <? $tabIDs["AV"] = $id++; ?>
                                <a class="nav-link" id="settings-av-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-av" href="#settings-av" data-option="AV" role="tab"
                                    aria-controls="settings-av" aria-selected="true">
                                    Audio/Video
                                </a>
                            </li>
                            <? if ((!$settings["IsDesktop"]) || ($settings['uiLevel'] > 2)) { ?>
                                                    <li class="nav-item">
                                                        <? $tabIDs["Localization"] = $id++; ?>
                                                        <a class="nav-link" id="settings-localization-tab" data-bs-toggle="tab"
                                                            data-bs-target="#settings-localization" href="#settings-localization" data-option="Localization" role="tab"
                                                            aria-controls="settings-localization" aria-selected="true">
                                                            Localization
                                                        </a>
                                                    </li>
                            <? } ?>
                            <li class="nav-item">
                                <? $tabIDs["UI"] = $id++; ?>
                                <a class="nav-link" id="settings-ui-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-ui" href="#settings-ui" data-option="UI" role="tab"
                                    aria-controls="settings-ui" aria-selected="true">
                                    UI
                                </a>
                            </li>
                            <? if ($settings["Platform"] != "MacOS") {
                                if ($uiLevel >= 1 || $tabId == "Email") {
                                    $tabIDs["Email"] = $id++; ?>
                                                                            <li class="nav-item">
                                                                                <a class="nav-link" id="settings-email-tab" data-bs-toggle="tab"
                                                                                    data-bs-target="#settings-email" href="#settings-email" data-option="Email"
                                                                                    role="tab" aria-controls="settings-email" aria-selected="true">
                                                                                    Email
                                                                                </a>
                                                                            </li>
                                                    <? }
                            } ?>
                            <? if ($uiLevel >= 1 || $tabId == "MQTT") { ?>
                                                    <li class="nav-item">
                                                        <? $tabIDs["MQTT"] = $id++; ?>
                                                        <a class="nav-link" id="settings-mqtt-tab" data-bs-toggle="tab"
                                                            data-bs-target="#settings-mqtt" href="#settings-mqtt" data-option="MQTT" role="tab"
                                                            aria-controls="settings-mqtt" aria-selected="true">
                                                            MQTT
                                                        </a>
                                                    </li>
                            <? } ?>
                            <li class="nav-item">
                                <? $tabIDs["Privacy"] = $id++; ?>
                                <a class="nav-link" id="settings-privacy-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-privacy" href="#settings-privacy" data-option="Privacy"
                                    role="tab" aria-controls="settings-privacy" aria-selected="true">
                                    Privacy
                                </a>
                            </li>
                            <? if ($uiLevel >= 1 || $tabId == "Output") { ?>
                                                    <li class="nav-item">
                                                        <? $tabIDs["Output"] = $id++; ?>
                                                        <a class="nav-link" id="settings-output-tab" data-bs-toggle="tab"
                                                            data-bs-target="#settings-output" href="#settings-output" data-option="Output"
                                                            role="tab" aria-controls="settings-output" aria-selected="true">
                                                            Input/Output
                                                        </a>
                                                    </li>
                            <? } ?>
                            <li class="nav-item">
                                <? $tabIDs["Logging"] = $id++; ?>
                                <a class="nav-link" id="settings-logs-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-logs" href="#settings-logs" data-option="Logging"
                                    role="tab" aria-controls="settings-logs" aria-selected="true">
                                    Logging
                                </a>
                            </li>
                            <li class="nav-item">
                                <? $tabIDs["Services"] = $id++; ?>
                                <a class="nav-link" id="settings-services-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-services" href="#settings-services" data-option="Services"
                                    role="tab" aria-controls="settings-services" aria-selected="true">
                                    Services
                                </a>
                            </li>
                            <? if ($uiLevel >= $storageUILevel || $tabId == "Storage") { ?>
                                                    <li class="nav-item">
                                                        <? $tabIDs["Storage"] = $id++; ?>
                                                        <a class="nav-link" id="settings-storage-tab" data-bs-toggle="tab"
                                                            data-bs-target="#settings-storage" href="#settings-storage" data-option="Storage"
                                                            role="tab" aria-controls="settings-storage" aria-selected="true">
                                                            Storage
                                                        </a>
                                                    </li>
                            <? } ?>
                            <li class="nav-item">
                                <? $tabIDs["System"] = $id++; ?>
                                <a class="nav-link" id="settings-system-tab" data-bs-toggle="tab"
                                    data-bs-target="#settings-system" href="#settings-system" data-option="System"
                                    role="tab" aria-controls="settings-system" aria-selected="true">
                                    System
                                </a>
                            </li>
                            <? if ($uiLevel >= 3 || $tabId == "Developer") { ?>
                                                    <li class="nav-item">
                                                        <? $tabIDs["Developer"] = $id++; ?>
                                                        <a class="nav-link" id="settings-developer-tab" data-bs-toggle="tab"
                                                            data-bs-target="#settings-developer" href="#settings-developer"
                                                            data-option="Developer" role="tab" aria-controls="settings-developer"
                                                            aria-selected="true">
                                                            Developer
                                                        </a>
                                                    </li>
                            <? } ?>
                        </ul>
                        <div id="settingsManagerTabsContent" class="tab-content">
                            <!-- Server-rendered so a spinner is on screen at the very first
                                 paint, before any JavaScript has run.  Removed once the
                                 per-tab panes (each with their own spinner) are built. -->
                            <div id="settingsTabsLoading" class="text-center p-4">
                                <div class="spinner-border spinner-danger spinner-lg" role="status">
                                    <span class="visually-hidden">Loading...</span>
                                </div>
                                <div class="mt-3 text-danger fw-bold">Loading settings...</div>
                            </div>
                        </div>

                        <br>
                        <? if ($uiLevel >= 1) { ?>
                                                <div class="backdrop">
                                                    <div class="row">
                                                        <div class="col-auto"><i class='fas fa-fw fa-graduation-cap ui-level-1'></i> - Advanced
                                                            Level Setting</div>
                                                        <? if ($uiLevel >= 2) { ?>
                                                                            <div class="col-auto"><i class='fas fa-fw fa-flask ui-level-2'></i> - Experimental Level
                                                                                Setting</div>
                                                        <? } ?>
                                                        <? if ($uiLevel >= 3) { ?>
                                                                            <div class="col-auto"><i class='fas fa-fw fa-code ui-level-3'></i> - Developer Level
                                                                                Setting</div>
                                                        <? } ?>
                                                    </div>

                                                </div>
                        <? } ?>

                    </div>
                </div>
            </div>

            <?php include 'common/footer.inc'; ?>

            <script>
                var activeTabNumber =
                    <?php

                    if (!array_key_exists($tabId, $tabIDs)) {
                        print $tabId;
                    } else {
                        print $tabIDs[$tabId];
                    }
                    ?>;
                
                var tabIDs = <?php echo json_encode($tabIDs); ?>;
if(location.hash){
    if( location.hash == '#settings-system'){ activeTabNumber = tabIDs["System"]}
    if( location.hash == '#settings-av'){ activeTabNumber = tabIDs["AV"]}
    if( location.hash == '#settings-localization'){ activeTabNumber = tabIDs["Localization"] }
    if( location.hash == '#settings-ui'){ activeTabNumber = tabIDs["UI"] }
    if( location.hash == '#settings-email'){ activeTabNumber = tabIDs["Email"] }
    if( location.hash == '#settings-mqtt'){ activeTabNumber = tabIDs["MQTT"] }
    if( location.hash == '#settings-privacy'){ activeTabNumber = tabIDs["Privacy"] }
    if( location.hash == '#settings-output'){ activeTabNumber = tabIDs["Output"] }
    if( location.hash == '#settings-logs'){ activeTabNumber = tabIDs["Logging"] }
    if( location.hash == '#settings-services'){ activeTabNumber = tabIDs["Services"]}
    if( location.hash == '#settings-storage'){ activeTabNumber = tabIDs["Storage"]}
    if( location.hash == '#settings-system'){ activeTabNumber = tabIDs["System"] }
    if( location.hash == '#settings-developer'){ activeTabNumber = tabIDs["Developer"] }
}
                var tabSpinnerHtml = '<div class="text-center p-4">' +
                    '<div class="spinner-border spinner-danger spinner-lg" role="status">' +
                    '<span class="visually-hidden">Loading...</span></div>' +
                    '<div class="mt-3 text-danger fw-bold">Loading settings...</div></div>';

                // 'loading' once a request is in flight, 'loaded' once the content is in
                // the DOM.  Keeps the prefetch and a click on the same tab from racing.
                var settingsTabState = {};

                function afterSettingsTabLoad() {
                    UpdateChildSettingsVisibility();
                    InitializeTimeInputs();
                    InitializeDateInputs();
                    SetupToolTips();
                }

                function loadSettingsTab(tabName, $tabContent, onSuccess) {
                    if (settingsTabState[tabName]) {
                        return;
                    }
                    settingsTabState[tabName] = 'loading';
                    $tabContent.html(tabSpinnerHtml);
                    $.ajax({
                        url: tabName + ".php"
                    }).done(function (data) {
                        settingsTabState[tabName] = 'loaded';
                        $tabContent.html(data);
                        if (onSuccess) onSuccess();
                    }).fail(function () {
                        delete settingsTabState[tabName];
                        var $error = $('<div class="text-danger p-3">' +
                            '<i class="fas fa-exclamation-triangle me-2"></i>Failed to load this settings page. ' +
                            '<a href="#" class="settingsTabRetry">Retry</a></div>');
                        $error.find('.settingsTabRetry').on('click', function (e) {
                            e.preventDefault();
                            loadSettingsTab(tabName, $tabContent, onSuccess);
                        });
                        $tabContent.html($error);
                    });
                }

                // A hash (or ?tab=) naming a tab that isn't present at this UI level
                // leaves activeTabNumber undefined, which would leave every pane hidden
                // and nothing loaded at all.  Fall back to the first tab.
                var settingsTabCount = $('#settingsManagerTabs .nav-link').length;
                if (typeof activeTabNumber !== 'number' || isNaN(activeTabNumber) ||
                    activeTabNumber < 0 || activeTabNumber >= settingsTabCount) {
                    activeTabNumber = 0;
                }

                var tabs = {};
                $('#settingsManagerTabs .nav-link').each(function (i) {
                    var tabName = $(this).attr('href').slice(1);
                    var dataOption = $(this).data('option');
                    var $tabContent = $('<div class="tab-pane fade" id="' + tabName + '" role="tabpanel" aria-labelledby="' + tabName + '-tab"></div>').html(tabSpinnerHtml);
                    $('#settingsManagerTabsContent').append($tabContent);
                    tabs[dataOption] = {
                        navEl: this,
                        tabNumber: i,
                        tabName: tabName,
                        $tabContent: $tabContent
                    };
                    if (i == activeTabNumber) {
                        $tabContent.addClass('show active');
                        $(this).addClass('active');
                    }
                });
                // Each pane now carries its own spinner, so the placeholder one that was
                // in the server-rendered HTML is no longer needed.
                $('#settingsTabsLoading').remove();

                // Load the visible tab first, then prefetch the rest in the background.
                // A tab clicked before its prefetch finishes keeps showing its spinner;
                // one clicked before its prefetch has started begins loading right away.
                $('#settingsManagerTabs a[data-bs-toggle="tab"]').on('shown.bs.tab', function (e) {
                    var tab = tabs[$(this).data('option')];
                    if (tab) {
                        loadSettingsTab(tab.tabName, tab.$tabContent, afterSettingsTabLoad);
                    }
                    if (($(this).attr("href") == '#settings-localization') &&
                        ($(this).parent().hasClass('active'))) {
                        UpdateCurrentTime();
                    } else if (statusTimeout != null) {
                        clearTimeout(statusTimeout);
                        statusTimeout = null;
                    }
                });

                function startSettingsTabLoading() {
                    $.each(tabs, function (dataOption, tab) {
                        if (tab.tabNumber == activeTabNumber) {
                            loadSettingsTab(tab.tabName, tab.$tabContent, function () {
                                afterSettingsTabLoad();
                                $.each(tabs, function (i, other) {
                                    loadSettingsTab(other.tabName, other.$tabContent, afterSettingsTabLoad);
                                });
                            });
                        }
                    });
                }

                // Wait for the browser to actually paint the spinners before firing any
                // request.  On a slow machine the whole page-load script runs in one long
                // main-thread block; without this the spinner is inserted and replaced
                // inside that same block and never reaches the screen.
                if (window.requestAnimationFrame) {
                    requestAnimationFrame(function () {
                        setTimeout(startSettingsTabLoading, 0);
                    });
                } else {
                    setTimeout(startSettingsTabLoading, 0);
                }
                
                // $.when.apply( undefined, tabRequests ).then(function() {
                //    $('#settingsManagerTabsContent>.spinner-border').hide();
                //     $.each(tabElements,function(i,$tabContent){
                //         if(i==activeTabNumber){
                //             $tabContent.addClass('show active');
                //         }
                
                //     });
                //     $('#settingsManagerTabs .nav-link').eq(activeTabNumber).addClass('active');
                //     $('a[data-bs-toggle="tab"]').on('shown.bs.tab', function (e) {
                //         if ($(this).attr("href") == '#settings-time') {
                //             UpdateCurrentTime();
                //         } else if (statusTimeout != null) {
                //             clearTimeout(statusTimeout);
                //             statusTimeout = null;
                //         }
                //     });
                //     UpdateChildSettingsVisibility();
                //     InitializeTimeInputs();
                //     InitializeDateInputs();
                // });
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
                        if (ui.newTab.find("a").attr("href") == 'settings-localization.php') {
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
/* $( function() {
    SetupToolTips();
}); */
/*
$('#tabs').show();
*/
</script>

</div>
</body>
</html>
