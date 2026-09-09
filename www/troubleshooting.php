<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'common.php';
    require_once 'config.php';
    include 'common/menuHead.inc'; ?>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title><? echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'help';
        include 'menu.inc'; ?>
        <div class="mainContainer">

            <h1 class="title">Troubleshooting</h1>
            <div class="pageContent">
                <div class="d-flex flex-wrap align-items-center justify-content-between gap-2 mb-2">
                    <h2 class="mb-0">Troubleshooting Commands</h2>
                    <button type="button" class="btn btn-outline-secondary btn-sm"
                        onclick="DownloadZip('Logs', this, 'Generating Support Bundle…');"
                        title="Downloads a zip containing FPP logs, configuration and the output of the troubleshooting commands on this page. Attach this to a support request. Takes around 10 seconds to build.">
                        <i class="fas fa-file-archive me-1"></i>Download Support Bundle (Logs / Config / Troubleshooting)
                    </button>
                </div>

                <ul class="nav nav-pills mb-3 pageContent-tabs" id="pills-tab" role="tablist">
                    <?
                    //LoadCommands
                    $troubleshootingCommandsLoaded = 0;
                    LoadTroubleShootingCommands();
                    $target_platforms = array('all', $settings['Platform']);

                    //Display Nav Tabs - one per group
                    foreach (array_keys($troubleshootingCommandGroups) as $commandGrpID) {
                        //Loop through groupings
                        //Display group if relevant for current platform
                        if (count(array_intersect($troubleshootingCommandGroups[$commandGrpID]["platforms"], $target_platforms)) > 0) {
                            echo "<li class=\"nav-item\" role=\"presentation\">";
                            echo "<button class=\"nav-link" . ($commandGrpID == array_key_first($troubleshootingCommandGroups) ? " active" : "") . "\" id=\"pills-" . $commandGrpID . "-tab\" data-bs-toggle=\"pill\" data-bs-target=\"#pills-" . $commandGrpID . "\" type=\"button\" role=\"tab\" aria-controls=\"pills-" . $commandGrpID . "\" aria-selected=\"true\">" . $troubleshootingCommandGroups[$commandGrpID]["grpDisplayTitle"] . "<span class=\"troubleshoot-tab-status\" id=\"tabstatus-" . $commandGrpID . "\"></span></button>";
                            echo "</li>";
                        }
                    }
                    ?>
                </ul>


                <div class="tab-content" id="pills-tabContent">
                    <?
                    ////Display Command Contents
                    foreach ($troubleshootingCommandGroups as $commandGrpID => $commandGrp) {
                        //Loop through groupings
                        //Display group if relevant for current platform
                        ${'hotlinks-' . $commandGrpID} = "<div class=\"container\"><div class=\"row mb-3\">";
                        if (count(array_intersect($troubleshootingCommandGroups[$commandGrpID]["platforms"], $target_platforms)) > 0) {
                            echo "<div class=\"tab-pane fade\"  id=\"pills-" . $commandGrpID . "\" role=\"tabpanel\" aria-labelledby=\"pills-" . $commandGrpID . "-tab\">";
                            ?>
                            <div id="troubleshooting-grp-<? echo $commandGrpID; ?>" class="backdrop">
                                <h4><? echo $commandGrp["grpDescription"]; ?></h4>
                                <div id="troubleshooting-hot-links-<? echo $commandGrpID; ?>"> </div>
                            </div>
                            <hr>
                            <div style="overflow: hidden; padding: 10px;">

                                <?
                                //Loop through commands in grp
                                echo "<div id=\"troubleshooting-results-" . $commandGrpID . "\">";
                                foreach ($commandGrp["commands"] as $commandKey => $commandID) {
                                    //Display command if relevant for current platform
                                    if (count(array_intersect($commandID["platforms"], $target_platforms)) > 0) {
                                        $commandTitle = $commandID["title"];
                                        $commandCmd = $commandID["cmd"];
                                        $commandDesc = $commandID["description"];
                                        $header = "header_" . $commandKey;
                                        ${'hotlinks-' . $commandGrpID} .= "<div class=\"col-md-3\"><a href=\"#$header\">$commandTitle</a><span id=\"hotlinkstatus_$commandKey\"></span></div>";
                                        ?>

                                    <a class="troubleshoot-anchor" name="<? echo $header ?>">.</a>
                                    <h3><? echo $commandTitle; ?><span id="<? echo ("status_" . $commandKey) ?>"></span>
                                    </h3>
                                    <strong>Command Description: </strong><? echo $commandDesc; ?>
                                        <br><strong>Command: </strong><? echo $commandCmd; ?>
                                        <pre
                                            id="<? echo ("command_" . $commandKey) ?>"><img src="./images/loading_spinner.gif" width="100" height="100"><i>Loading...</i></pre>
                                        <hr>
                                        <?
                                    }
                                }
                                ${'hotlinks-' . $commandGrpID} .= "</div></div>";
                                ?>
                        </div>
                    </div>
                </div>
                <?

                        }

                    }

                    ?>
        </div>


    </div>
    <?php include 'common/footer.inc'; ?>
    </div>
    <button type="button"
        class="back2top btn btn-danger fw-bold rounded-pill position-fixed bottom-0 end-0 m-3 z-3 opacity-50">Back to
        top</button>

    <script type="application/javascript">

        /*
         * Anchors are dynamically via ajax thus auto scrolling if anchor is in url
         * will fail.  This will workaround that problem by forcing a scroll
         * afterward dynamic content is loaded.
         */
        /*
         * Verdict highlighting.
         *
         * The commands emit plain text and that exact text is what
         * troubleshootingText.php puts in the support bundle -- nothing here
         * changes a byte of it.  This only decorates it on screen, so a
         * single failure buried sixty lines into an output does not have to
         * be found by reading every line.
         *
         * The diagnostic scripts mark each verdict with a leading
         * [PASS]/[WARN]/[FAIL]/[INFO]/[SKIP].  Commands that just print raw
         * state carry no markers and render exactly as they always have.
         */
        var troubleshootMarkerClasses = {
            FAIL: 'bg-danger-subtle text-danger-emphasis fw-bold',
            WARN: 'bg-warning-subtle text-warning-emphasis fw-bold',
            PASS: 'text-success-emphasis',
            INFO: 'text-body-secondary',
            SKIP: 'text-body-secondary'
        };

        function appendTroubleshootSpan(parent, text, className) {
            var span = document.createElement('span');
            if (className) {
                span.className = className;
            }
            span.textContent = text;
            parent.appendChild(span);
        }

        function renderTroubleshootOutput(pre, data) {
            /*
             * Built from text nodes rather than innerHTML.  Command output is
             * plain text and must never be parsed as markup: assigning it to
             * innerHTML meant anything a tool printed in angle brackets - a
             * <unavailable> value, a <node> name in gst-inspect output - was
             * treated as an unknown tag and silently vanished from the page.
             */
            pre.textContent = '';

            var counts = { FAIL: 0, WARN: 0, PASS: 0 };
            var lines = String(data).split('\n');
            // Severity of the verdict whose explanation the indented lines
            // below it belong to, so the "what to do about it" text stays
            // visually attached to the failure it explains.
            var explaining = '';

            for (var i = 0; i < lines.length; i++) {
                var line = lines[i];
                var marker = /^\[(FAIL|WARN|PASS|INFO|SKIP)\]/.exec(line);

                if (marker) {
                    var kind = marker[1];
                    if (counts[kind] !== undefined) {
                        counts[kind]++;
                    }
                    appendTroubleshootSpan(pre, line, troubleshootMarkerClasses[kind]);
                    explaining = (kind === 'FAIL' || kind === 'WARN') ? kind : '';
                } else if (explaining && /^\s+\S/.test(line)) {
                    // Coloured but not banded: banding every follow-on line
                    // would turn half the output into a block of red and lose
                    // the verdict lines in it.
                    var indent = line.match(/^\s*/)[0];
                    pre.appendChild(document.createTextNode(indent));
                    appendTroubleshootSpan(pre, line.slice(indent.length),
                        explaining === 'FAIL' ? 'text-danger-emphasis' : 'text-warning-emphasis');
                } else {
                    explaining = '';
                    if (/^\s*(===|---) .+ (===|---)\s*$/.test(line)) {
                        appendTroubleshootSpan(pre, line, 'fw-bold');
                    } else if (/^Result: /.test(line)) {
                        appendTroubleshootSpan(pre, line,
                            counts.FAIL > 0 ? 'text-danger-emphasis fw-bold'
                                : counts.WARN > 0 ? 'text-warning-emphasis fw-bold'
                                    : 'text-success-emphasis fw-bold');
                    } else {
                        pre.appendChild(document.createTextNode(line));
                    }
                }

                if (i < lines.length - 1) {
                    pre.appendChild(document.createTextNode('\n'));
                }
            }

            return counts;
        }

        // One badge shape, used beside the heading, beside the hot link and on
        // the tab, so the same verdict reads the same in all three places.
        // The compact form drops the wording to fit a tab or a link, but keeps
        // it in the title so the badge never depends on colour alone.
        function troubleshootBadges(counts, compact) {
            var kinds = [
                { n: counts.FAIL, cls: 'text-bg-danger', one: 'failure', many: 'failures' },
                { n: counts.WARN, cls: 'text-bg-warning', one: 'warning', many: 'warnings' }
            ];
            var html = '';
            for (var i = 0; i < kinds.length; i++) {
                if (kinds[i].n > 0) {
                    var label = kinds[i].n + ' ' + (kinds[i].n === 1 ? kinds[i].one : kinds[i].many);
                    html += '<span class="badge ' + kinds[i].cls + ' ms-2" title="' + label + '">' +
                        (compact ? kinds[i].n : label) + '</span>';
                }
            }
            // Only when there is nothing to report: an all-clear badge next to
            // a failure badge would just be noise.
            if (!html && counts.PASS > 0) {
                var passed = counts.PASS + ' passed';
                html += '<span class="badge text-bg-success ms-2" title="' + passed + '">' +
                    (compact ? counts.PASS : passed) + '</span>';
            }
            return html;
        }

        function setTroubleshootBadge(id, counts, compact) {
            var el = document.querySelector('#' + id);
            if (el) {
                el.innerHTML = troubleshootBadges(counts, compact);
            }
        }

        // Roll the group's tab badge up from the commands that have reported
        // so far.  Commands run when their tab is first opened, so a tab shows
        // a badge once it has been looked at - never a stale one from a run
        // that has not happened.
        function updateTroubleshootTabBadge(commandGrpID) {
            var pane = document.querySelector('#pills-' + commandGrpID);
            if (!pane) {
                return;
            }
            var totals = { FAIL: 0, WARN: 0, PASS: 0 };
            pane.querySelectorAll('pre[data-troubleshoot-fail]').forEach(function (pre) {
                totals.FAIL += parseInt(pre.dataset.troubleshootFail, 10) || 0;
                totals.WARN += parseInt(pre.dataset.troubleshootWarn, 10) || 0;
                totals.PASS += parseInt(pre.dataset.troubleshootPass, 10) || 0;
            });
            setTroubleshootBadge('tabstatus-' + commandGrpID, totals, true);
        }

        function ShowTroubleshootResult(commandKey, commandGrpID, data) {
            var pre = document.querySelector('#command_' + commandKey);
            if (!pre) {
                return;
            }

            var counts = renderTroubleshootOutput(pre, data);
            pre.dataset.troubleshootFail = counts.FAIL;
            pre.dataset.troubleshootWarn = counts.WARN;
            pre.dataset.troubleshootPass = counts.PASS;

            setTroubleshootBadge('status_' + commandKey, counts, false);
            setTroubleshootBadge('hotlinkstatus_' + commandKey, counts, true);
            updateTroubleshootTabBadge(commandGrpID);
        }

        function fixScroll() {
            // Remove the # from the hash, as different browsers may or may not include it
            var hash = location.hash.replace('#', '');

            if (hash != '') {
                var elements = document.getElementsByName(hash);
                if (elements.length > 0) {
                    elements[0].scrollIntoView();
                }
            }
        }


    <?
    foreach ($troubleshootingCommandGroups as $commandGrpID => $commandGrp) {
        //Create logic for grp if relevant for platform
        if (count(array_intersect($commandGrp["platforms"], $target_platforms)) > 0) {
            echo ("document.querySelector(\"#troubleshooting-hot-links-" . $commandGrpID . "\").innerHTML ='" . ${'hotlinks-' . $commandGrpID} . "'; \n");
            ?> function dispTroubleTab<? echo $commandGrpID; ?>() {
                    <?
                    foreach ($commandGrp["commands"] as $commandKey => $commandID) {
                        //Run command if relevant for current platform
                        if (count(array_intersect($commandID["platforms"], $target_platforms)) > 0) {
                            $url = "./troubleshootingHelper.php?key=" . urlencode($commandKey);
                            ?>
                            $.ajax({
                                url: "<?php echo $url ?>",
                                type: 'GET',
                                success: function (data) {
                                    ShowTroubleshootResult("<?php echo $commandKey ?>", "<?php echo $commandGrpID ?>", data);
                                    fixScroll();
                                },
                                error: function () {
                                    DialogError('Failed to query command', "Error: Unable to query for <?php echo $commandKey ?>");
                                }
                            });

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                <?
                        }
                    } ?>}<?
        }
    }
    ?>

        function pageSpecific_PageLoad_DOM_Setup() {
            document.getElementById("pills-<? echo array_key_first($troubleshootingCommandGroups) ?>").classList.add('active');
            document.getElementById("pills-<? echo array_key_first($troubleshootingCommandGroups) ?>").classList.add('show');
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {

            //Setup action functions
            // commands for selected tab
            $('.nav-link').on('shown.bs.tab', function () {
                var id = $(this).attr("id");
                var cmdgrp = id.replace("pills-", "").replace("-tab", "");

                var fn_string = "dispTroubleTab".concat(cmdgrp);
                var fn = window[fn_string];
                if (typeof fn === "function") fn();
            });

            //scroll to top
            $(".back2top").click(() => $("html, body").animate({ scrollTop: 0 }, "slow") && false);
            $(window).scroll(function () {
                if ($(this).scrollTop() > 100) $('.back2top').fadeIn();
                else $('.back2top').fadeOut();
            });

            //default to show first tab in grp array active
            if (!location.hash) {
                $('#pills-tab button[id="pills-<? echo array_key_first($troubleshootingCommandGroups) ?>-tab"]').tab('show');
                dispTroubleTab<? echo array_key_first($troubleshootingCommandGroups) ?>();
            }

            //if hash is tab
            if (location.hash.includes('pills-')) {
                CmdGrp = location.hash.replace('#pills-', '').toString();
                var fn_string = "dispTroubleTab".concat(CmdGrp);
                var fn = window[fn_string];
                if (typeof fn === "function") fn();

            }

            //if hash is location on a tab, derive correct tab and move to tab and hash
            var hotlinks = [];
            $('a.troubleshoot-anchor').each(function () {
                hotlinks.push($(this).attr('name'));
            });
            if (location.hash && hotlinks.includes(location.hash.replace('#', ''))) {
                orig_hotlink = location.hash;

                //find ancestor with class tab-pane
                target_tab = $('a.troubleshoot-anchor[name="' + location.hash.replace('#', '') + '"]').closest('.tab-pane').attr('id')
                if (target_tab) {
                    bootstrap.Tab.getOrCreateInstance(
                        document.querySelector('[data-bs-target="#' + target_tab + '"]')
                    ).show();
                    window.location.hash = orig_hotlink;
                    setTimeout(function () {
                        SetTablePageHeader_ZebraPin();
                        float_fppStickyThead();
                        scrollToTop();
                    }, 50);
                }
            }
        }


    </script>

</body>

</html>