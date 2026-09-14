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
            if (isHidden()) {
                StopCurrentTime();
            } else if (ClockWanted()) {
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

        // Not 'statusTimeout': that is fpp.js's global for the header status poll.
        var clockTimeout = null;

        // The clock only runs while the Localization tab is showing.  Checked
        // again when each api/time reply arrives, because a tab switch or a
        // hidden page can happen while the request is in flight and there is
        // no pending timer to cancel at that moment.
        function ClockWanted() {
            return !isHidden() && $('#settings-localization-tab').hasClass('active');
        }

        function StopCurrentTime() {
            if (clockTimeout != null) {
                clearTimeout(clockTimeout);
                clockTimeout = null;
            }
        }

        function UpdateCurrentTime() {
            StopCurrentTime();
            $.get('api/time', function (data) {
                $('#currentTime').html(data.time);
                if (ClockWanted() && clockTimeout == null) {
                    clockTimeout = setTimeout(UpdateCurrentTime, 1000);
                }
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
// ?tab[]=x would hand array_key_exists() an array and fatal the page.
if (isset($_GET['tab']) && is_string($_GET['tab'])) {
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
                        <div id="settingsSearchBar" class="row g-2 align-items-center mb-3">
                            <div class="col-12 col-md-6 col-lg-4">
                                <div class="input-group">
                                    <span class="input-group-text"><i class="fas fa-search"></i></span>
                                    <input type="search" id="settingsSearch" class="form-control"
                                        placeholder="Search all settings&hellip;" autocomplete="off"
                                        aria-label="Search settings">
                                </div>
                            </div>
                            <div class="col-auto">
                                <!-- Flex instead of Bootstrap's floated checkbox so the label
                                     stays centred against FPP's enlarged checkbox. -->
                                <div class="form-check d-flex align-items-center gap-2 mb-0 ps-0">
                                    <input class="form-check-input float-none m-0" type="checkbox" id="settingsSearchHelp">
                                    <label class="form-check-label" for="settingsSearchHelp"
                                        title="Also match the text behind each setting's help icon">Include tooltip text</label>
                                </div>
                            </div>
                            <span id="settingsSearchCount" class="col-auto fpp-text-muted d-none" aria-live="polite"></span>
                        </div>
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
                        <div id="settingsSearchNoResults" class="fpp-text-muted p-3 d-none" role="status">
                            <i class="fas fa-search me-2"></i>No settings match
                            "<span id="settingsSearchNoResultsTerm"></span>".
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
                // Only ever an integer: ?tab= is user input and this lands
                // inside a <script>, so an unknown tab falls back to the first.
                var activeTabNumber = <?php print array_key_exists($tabId, $tabIDs) ? (int) $tabIDs[$tabId] : 0; ?>;
                
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
                    applySettingsSearch();
                }

                // Settings search.  With a term entered, every tab pane is shown
                // stacked under a heading naming its tab, pruned down to just the
                // rows whose label, setting name, option text or field value
                // contain the term.  Help tooltips are only searched when the
                // "Include help text" box is ticked; on by default they surface
                // every setting whose blurb merely mentions the word.  Hiding is
                // done with a dedicated class rather than .hide()/.show() so rows
                // the page itself keeps hidden (child settings of an unchecked
                // parent) are left exactly as they were when the search is cleared.
                var settingsSearchStorageKey = 'fppSettingsSearch';
                // Static row text is extracted once per row element; a reloaded
                // tab brings new elements so the WeakMap drops the old ones.
                var settingsRowTextCache = new WeakMap();

                // Visible text of a row, skipping the inline <script> blocks that
                // PrintSetting emits inside checkbox and modal rows (otherwise
                // "function", "checked", "show"... match every row on the page).
                // Field values are read live since the user can edit them in the
                // results; password and hidden inputs are left out.
                function settingsRowText(row) {
                    var text = settingsRowTextCache.get(row);
                    if (text === undefined) {
                        var parts = [row.id.replace(/Row$/, '')];
                        var walker = document.createTreeWalker(row, NodeFilter.SHOW_TEXT, {
                            acceptNode: function (node) {
                                var tag = node.parentNode.tagName;
                                return (tag === 'SCRIPT' || tag === 'STYLE') ?
                                    NodeFilter.FILTER_REJECT : NodeFilter.FILTER_ACCEPT;
                            }
                        });
                        while (walker.nextNode()) {
                            parts.push(walker.currentNode.nodeValue);
                        }
                        text = parts.join(' ').toLowerCase();
                        settingsRowTextCache.set(row, text);
                    }
                    row.querySelectorAll('input:not([type=checkbox]):not([type=radio]):not([type=password]):not([type=hidden]), textarea')
                        .forEach(function (el) { text += ' ' + (el.value || '').toLowerCase(); });
                    return text;
                }

                // True when the page itself hides the row: an inactive child
                // setting (inline display:none from Update<setting>Children), or
                // a collapsed/hidden wrapper around it.  Checked from attributes
                // rather than offsetParent so no layout is forced; laying out 13
                // stacked panes costs ~150ms on a Pi.
                function settingsRowHiddenByPage(row, pane) {
                    for (var el = row; el && el !== pane; el = el.parentNode) {
                        if (el.style.display === 'none' || el.classList.contains('d-none') ||
                            el.classList.contains('hidden') ||
                            (el.classList.contains('collapse') && !el.classList.contains('show'))) {
                            return true;
                        }
                    }
                    return false;
                }

                // Only the real help tip (PrintToolTip's <setting>_tip span), not
                // the "Advanced Level Setting" titles on the UI level icons.  The
                // tip is HTML, so strip the tags before matching.
                function settingsRowHelpText(row) {
                    var text = '';
                    row.querySelectorAll('[id$="_tip"]').forEach(function (el) {
                        text += ' ' + (el.getAttribute('data-bs-title') ||
                            el.getAttribute('data-bs-original-title') ||
                            el.getAttribute('data-bs-tooltip-title') ||
                            el.getAttribute('title') || '');
                    });
                    return text.replace(/<[^>]*>/g, ' ').toLowerCase();
                }

                // Hides everything in $pane that is not a matched row or on the
                // path from the pane down to one.  Walks up from the matches, so
                // the interior of a non-matching subtree is never visited.  Bare
                // text nodes next to hidden siblings are wrapped so they can be
                // hidden too, and unwrapped again when the search is cleared.
                function pruneToSettingsMatches($pane, matchedRows) {
                    var pane = $pane[0];
                    var keep = new Set();
                    var matched = new Set(matchedRows);
                    matchedRows.forEach(function (row) {
                        for (var el = row.parentNode; el && el !== pane; el = el.parentNode) {
                            keep.add(el);
                        }
                    });
                    keep.add(pane);

                    keep.forEach(function (container) {
                        Array.prototype.slice.call(container.childNodes).forEach(function (node) {
                            if (node.nodeType === Node.TEXT_NODE) {
                                if (node.nodeValue.trim() !== '') {
                                    $(node).wrap('<span class="settingsSearchHide settingsSearchHiddenText"></span>');
                                }
                                return;
                            }
                            if (node.nodeType !== Node.ELEMENT_NODE || keep.has(node) || matched.has(node)) {
                                return;
                            }
                            // A table's header/footer stays with any kept rows.
                            if ((node.tagName === 'THEAD' || node.tagName === 'TFOOT') && keep.has(container)) {
                                return;
                            }
                            node.classList.add('settingsSearchHide');
                        });

                        // A group's <h2> is a sibling of its settingsTable, not a
                        // parent.  For each kept child, pull back the nearest heading
                        // before it, skipping over anything else that was just hidden
                        // (a note between the heading and its table), but stopping at
                        // the previous kept element so a heading is never stolen from
                        // another group.
                        Array.prototype.slice.call(container.children).forEach(function (child) {
                            if (!keep.has(child) && !matched.has(child)) {
                                return;
                            }
                            for (var prev = child.previousElementSibling; prev; prev = prev.previousElementSibling) {
                                if (/^H[1-6]$/.test(prev.tagName)) {
                                    prev.classList.remove('settingsSearchHide');
                                    break;
                                }
                                if (!prev.classList.contains('settingsSearchHide')) {
                                    break;
                                }
                            }
                        });
                    });
                }

                function clearSettingsSearchMarkup() {
                    var $manager = $('#settingsManager');
                    $manager.find('.settingsSearchHiddenText').contents().unwrap();
                    $manager.find('.settingsSearchHide').removeClass('settingsSearchHide');
                    $manager.find('.settingsSearchMatch').removeClass('settingsSearchMatch');
                    $manager.find('.settingsSearchTabHeading').remove();
                }

                function applySettingsSearch() {
                    var rawTerm = ($('#settingsSearch').val() || '').trim();
                    var term = rawTerm.toLowerCase();
                    var includeHelp = $('#settingsSearchHelp').is(':checked');
                    var $manager = $('#settingsManager');
                    var $panes = $('#settingsManagerTabsContent .tab-pane');

                    try {
                        sessionStorage.setItem(settingsSearchStorageKey,
                            JSON.stringify({ term: rawTerm, help: includeHelp }));
                    } catch (e) { }

                    clearSettingsSearchMarkup();

                    if (term === '') {
                        $manager.removeClass('settingsSearchActive');
                        $('#settingsManagerTabs').removeClass('d-none');
                        // Back to Bootstrap's normal one-pane-at-a-time state.
                        var activeHref = $('#settingsManagerTabs .nav-link.active').attr('href');
                        $panes.not(activeHref).removeClass('show active');
                        $('#settingsSearchCount').addClass('d-none');
                        $('#settingsSearchNoResults').addClass('d-none');
                        return;
                    }

                    // Every pane visible at once, using Bootstrap's own classes.
                    $manager.addClass('settingsSearchActive');
                    $('#settingsManagerTabs').addClass('d-none');
                    $panes.addClass('show active');

                    var total = 0;
                    var allLoaded = true;
                    $.each(tabs, function (dataOption, tab) {
                        var $pane = tab.$tabContent;
                        var $heading = $('<h2 class="settingsSearchTabHeading fs-4 fw-bold mt-4 mb-2 pb-1 border-bottom">' +
                            '<button type="button" class="btn btn-link p-0 fs-4 fw-bold text-decoration-none"></button></h2>');
                        $heading.find('button').text($(tab.navEl).text().trim()).on('click', function () {
                            $('#settingsSearch').val('');
                            applySettingsSearch();
                            $(tab.navEl).tab('show');
                            tab.navEl.focus();
                        });

                        // Still loading: keep the spinner in view under its heading
                        // and let afterSettingsTabLoad() re-run the filter later.
                        if (settingsTabState[tab.tabName] !== 'loaded') {
                            allLoaded = false;
                            $pane.prepend($heading);
                            return;
                        }

                        // Rows are PrintSetting rows (<name>Row), anything in a
                        // settingsTable, and the Privacy tab's table rows.
                        var matchedRows = [];
                        $pane.find('.row[id$="Row"], .settingsTable .row, tr.privacyRow').each(function () {
                            if ($(this).parentsUntil($pane, '.row').length > 0) {
                                return; // a layout row nested inside a setting row
                            }
                            if (settingsRowHiddenByPage(this, $pane[0])) {
                                return;
                            }
                            if (settingsRowText(this).indexOf(term) !== -1 ||
                                (includeHelp && settingsRowHelpText(this).indexOf(term) !== -1)) {
                                matchedRows.push(this);
                            }
                        });

                        if (matchedRows.length > 0) {
                            matchedRows.forEach(function (row) { row.classList.add('settingsSearchMatch'); });
                            pruneToSettingsMatches($pane, matchedRows);
                            $pane.prepend($heading);
                        } else {
                            $pane.addClass('settingsSearchHide');
                        }
                        total += matchedRows.length;
                    });

                    if (allLoaded) {
                        $('#settingsSearchCount').text(total + (total === 1 ? ' match' : ' matches'));
                    } else {
                        $('#settingsSearchCount').text('Searching…');
                    }
                    $('#settingsSearchCount').removeClass('d-none');
                    $('#settingsSearchNoResultsTerm').text(rawTerm);
                    $('#settingsSearchNoResults').toggleClass('d-none', !(allLoaded && total === 0));
                }

                $('#settingsSearchHelp').on('change', applySettingsSearch);
                $('#settingsSearch').on('input', applySettingsSearch)
                    .on('keydown', function (e) {
                        if (e.key === 'Escape' && $(this).val() !== '') {
                            $(this).val('');
                            applySettingsSearch();
                        }
                    });
                // Ticking a parent setting inside the results shows or hides its
                // children (Update<setting>Children runs row.show()/hide() from
                // the save callback, so a change handler would fire too early).
                // Watch for those inline style flips instead and re-filter.
                var settingsSearchRefilterTimer = null;
                new MutationObserver(function (mutations) {
                    if (!$('#settingsManager').hasClass('settingsSearchActive') || settingsSearchRefilterTimer) {
                        return;
                    }
                    for (var i = 0; i < mutations.length; i++) {
                        if (mutations[i].target.id && /Row$/.test(mutations[i].target.id)) {
                            settingsSearchRefilterTimer = setTimeout(function () {
                                settingsSearchRefilterTimer = null;
                                applySettingsSearch();
                            }, 0);
                            return;
                        }
                    }
                }).observe(document.getElementById('settingsManagerTabsContent'),
                    { attributes: true, attributeFilter: ['style'], subtree: true });
                // Survive the reload a reloadUI setting triggers.
                try {
                    var saved = JSON.parse(sessionStorage.getItem(settingsSearchStorageKey) || 'null');
                    if (saved && saved.term) {
                        $('#settingsSearch').val(saved.term);
                        $('#settingsSearchHelp').prop('checked', !!saved.help);
                    }
                } catch (e) { }

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
                if ($('#settingsSearch').val() !== '') {
                    applySettingsSearch(); // a term restored from the last visit
                }

                // Load the visible tab first, then prefetch the rest in the background.
                // A tab clicked before its prefetch finishes keeps showing its spinner;
                // one clicked before its prefetch has started begins loading right away.
                $('#settingsManagerTabs a[data-bs-toggle="tab"]').on('shown.bs.tab', function (e) {
                    var tab = tabs[$(this).data('option')];
                    if (tab) {
                        loadSettingsTab(tab.tabName, tab.$tabContent, afterSettingsTabLoad);
                    }
                    // (The old check for .active on the <li> was never true
                    // under Bootstrap 5, so the clock never started.)
                    if ($(this).attr("href") == '#settings-localization') {
                        UpdateCurrentTime();
                    } else {
                        StopCurrentTime();
                    }
                });

                function startSettingsTabLoading() {
                    $.each(tabs, function (dataOption, tab) {
                        if (tab.tabNumber == activeTabNumber) {
                            loadSettingsTab(tab.tabName, tab.$tabContent, function () {
                                afterSettingsTabLoad();
                                // shown.bs.tab never fires for the initially
                                // active tab, so start the clock here (unless the
                                // user already clicked away while it loaded).
                                if (tab.tabName == 'settings-localization' && ClockWanted()) {
                                    UpdateCurrentTime();
                                }
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
                
</script>

</div>
</body>
</html>
