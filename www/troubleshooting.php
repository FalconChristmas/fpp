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
    <style>
        .back2top {
            position: fixed;
            bottom: 10px;
            right: 20px;
            z-index: 99;
            background: red;
            color: #fff;
            border-radius: 30px;
            padding: 15px;
            font-weight: bold;
            line-height: normal;
            border: none;
            opacity: 0.5;
        }
    </style>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'help';
        include 'menu.inc'; ?>
        <div class="mainContainer">

            <h1 class="title">Troubleshooting</h1>
            <div class="pageContent">
                <h2>Troubleshooting Commands</h2>

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
                            echo "<button class=\"nav-link" . ($commandGrpID == array_key_first($troubleshootingCommandGroups) ? " active" : "") . "\" id=\"pills-" . $commandGrpID . "-tab\" data-bs-toggle=\"pill\" data-bs-target=\"#pills-" . $commandGrpID . "\" type=\"button\" role=\"tab\" aria-controls=\"pills-" . $commandGrpID . "\" aria-selected=\"true\">" . $troubleshootingCommandGroups[$commandGrpID]["grpDisplayTitle"] . "</button>";
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
                        if (count(array_intersect($troubleshootingCommandGroups[$commandGrpID]["platforms"], $target_platforms)) > 0) {
                            ${'hotlinks-' . $commandGrpID} = "<div class=\"container\"><div class=\"row mb-3\">";
                            echo "<div class=\"tab-pane fade\"  id=\"pills-" . $commandGrpID . "\" role=\"tabpanel\" aria-labelledby=\"pills-" . $commandGrpID . "-tab\">";
                            ?>
                            <div id="troubleshooting-grp-<? echo $commandGrpID; ?>" class="backdrop">
                                <h4><? echo $commandGrp["grpDescription"]; ?></h4>
                                <div id="troubleshooting-hot-links-<? echo $commandGrpID; ?>"> </div>
                            </div>
                            <hr>
                            <div style="overflow: hidden; padding: 10px;">

                                <?
                        }
                        //Loop through commands in grp
                        echo "<div id=\"troubleshooting-results-" . $commandGrpID . "\">";
                        foreach ($commandGrp["commands"] as $commandKey => $commandID) {
                            //Display command if relevant for current platform
                            if (count(array_intersect($commandID["platforms"], $target_platforms)) > 0) {
                                $commandTitle = $commandID["title"];
                                $commandCmd = $commandID["cmd"];
                                $commandDesc = $commandID["description"];
                                $header = "header_" . $commandKey;
                                ${'hotlinks-' . $commandGrpID} .= "<div class=\"col-md-3\"><a href=\"#$header\">$commandTitle</a></div>";
                                ?>

                                    <a class="troubleshoot-anchor" name="<? echo $header ?>">.</a>
                                    <h3><? echo $commandTitle; ?></h3>
                                    <strong><? echo "Command Description: </strong>" . $commandDesc; ?>
                                        <strong><? echo "<br>Command: </strong>" . $commandCmd; ?></strong>
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

                    ?>
        </div>


    </div>
    <?php include 'common/footer.inc'; ?>
    </div>
    <button type="button" class="back2top">Back to top</button>

    <script type="application/javascript">

        /*
         * Anchors are dynamically via ajax thus auto scrolling if anchor is in url
         * will fail.  This will workaround that problem by forcing a scroll
         * afterward dynamic content is loaded.
         */
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
                                    document.querySelector("#<?php echo ("command_" . $commandKey) ?>").innerHTML = data;
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