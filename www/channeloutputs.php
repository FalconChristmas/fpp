<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    require_once 'universeentry.php';
    if (file_exists(__DIR__ . "/fppdefines.php")) {
        include_once __DIR__ . '/fppdefines.php';
    } else {
        include_once __DIR__ . '/fppdefines_unknown.php';
    }
    include 'common/menuHead.inc';

    //$settings['Platform'] = "Raspberry Pi"; // Uncomment for testing
//$settings['Platform'] = "BeagleBone Black"; // Uncomment for testing
//$settings['SubPlatform'] = "BeagleBone Black"; // Uncomment for testing
    ?>
    <script>
        var currentCapeName = '';
    </script>

    <style>
        .outputTable {
            background: #F0F0F0;
            width: 100%;
            border-spacing: 0px;
            border-collapse: collapse;
        }

        .outputTable th {
            vertical-align: bottom;
            text-align: center;
            border: solid 2px #888888;
            font-size: 0.8em;
        }

        .outputTable td {
            text-align: center;
            padding: 0px 9px 0px 0px;
        }

        .outputTable tbody tr td input[type=text] {
            text-align: center;
            width: 100%;
        }

        .outputTable tbody tr td input[type=number] {
            text-align: center;
            width: 100%;
        }

        h2.divider {
            width: 100%;
            text-align: left;
            border-bottom: 2px solid #000;
            line-height: 0.1em;
            margin: 12px 0 10px;
        }

        h2.divider span {
            background: #f5f5f5;
            padding: 0 10px;
        }

        /* prevent table header from scrolling: */
        #PixelString {
            text-align: left;
            position: relative;
            /* required for th sticky to work */
        }

        #PixelString thead th {
            font-weight: bold;
            background: #fff;
            /* prevent add/delete circular buttons from showing through */
            position: sticky;
            top: 0;
            z-index: 20;
            /* draw table header on top of add/delete buttons */
            padding: 8px 0 0 5px;
            border-bottom: 2px solid #d5d7da
        }

        <?
        if ($settings['BeaglePlatform']) {
            //  BBB only supports ws2811 at this point
            ?>
            #PixelString tr>th:nth-of-type(2),
            #PixelString tr>td:nth-of-type(2),
            div[aria-labelledby="PixelString"] .floatThead-table tr>th:nth-of-type(2) {
                display: none;
            }

            <?
        }
        if ((isset($settings['cape-info']) && $settings['cape-info']['id'] == "Unsupported")) {
            // don't support virtual strings
            ?>
            #PixelString tr>th:nth-of-type(3),
            div[aria-labelledby="PixelString"] .floatThead-table tr>th:nth-of-type(3) #PixelString tr>td:nth-of-type(3) {
                display: none;
            }

            <?
        }
        ?>


        #ModelPixelStrings_Output_0 tr>th:nth-of-type(2),
        #ModelPixelStrings_Output_0 tr>td:nth-of-type(2) {
            display: none;
        }
    </style>

    <?
    $currentCape = "";
    $currentCapeInfo = json_decode("{ \"provides\": [\"all\"]}", true);
    if (isset($settings['cape-info'])) {
        $currentCapeInfo = $settings['cape-info'];
        $currentCape = $currentCapeInfo["id"];
        echo "<!-- current cape is " . $currentCape . "-->\n";
        printf(
            "<script>\ncurrentCapeName = '%s';\n</script>\n",
            $currentCapeInfo["name"]
        );
    }
    if (!isset($currentCapeInfo['provides'])) {
        $currentCapeInfo['provides'][] = "all";
        if (isset($settings['FalconHardwareDetected']) && ($settings['FalconHardwareDetected'] == 1)) {
            $currentCapeInfo['provides'][] = "fpd";
        }
    } else if (isset($settings["showAllOptions"]) && $settings["showAllOptions"] == 1) {
        $currentCapeInfo['provides'][] = "all";
        $currentCapeInfo['provides'][] = "fpd";
    } else if ($uiLevel >= 2) {
        $currentCapeInfo['provides'][] = "all";
    } else if ($uiLevel == 1) {
        if (!isset($currentCapeInfo['provides']) || count($currentCapeInfo['provides']) == 0) {
            $currentCapeInfo['provides'][] = "all";
        } else {
            $currentCapeInfo['provides'][] = "udp";
        }
    }
    if (isset($settings['cape-info'])) {
        // update provides with additional stuff computed above
        $settings['cape-info']["provides"] = $currentCapeInfo["provides"];
    }
    ?>

    <script language="Javascript">

        var channelOutputs = [];
        var channelOutputsLookup = [];
        var currentTabType = 'UDP';

        /////////////////////////////////////////////////////////////////////////////

        function UpdateChannelOutputLookup() {
            var i = 0;
            for (i = 0; i < channelOutputs.channelOutputs.length; i++) {
                channelOutputsLookup[channelOutputs.channelOutputs[i].type + "-Enabled"] = channelOutputs.channelOutputs[i].enabled;
                if (channelOutputs.channelOutputs[i].type == "LEDPanelMatrix") {
                    channelOutputsLookup["LEDPanelMatrix"] = channelOutputs.channelOutputs[i];

                    var p = 0;
                    for (p = 0; p < channelOutputs.channelOutputs[i].panels.length; p++) {
                        var r = channelOutputs.channelOutputs[i].panels[p].row;
                        var c = channelOutputs.channelOutputs[i].panels[p].col;

                        channelOutputsLookup["LEDPanelOutputNumber_" + r + "_" + c]
                            = channelOutputs.channelOutputs[i].panels[p];
                        channelOutputsLookup["LEDPanelPanelNumber_" + r + "_" + c]
                            = channelOutputs.channelOutputs[i].panels[p];
                        channelOutputsLookup["LEDPanelColorOrder_" + r + "_" + c]
                            = channelOutputs.channelOutputs[i].panels[p];
                    }
                }
            }
        }

        function GetChannelOutputConfig() {
            var config = new Object();

            config.channelOutputs = [];

            var lpc = GetLEDPanelConfig();
            config.channelOutputs.push(lpc);

            channelOutputs = config;
            UpdateChannelOutputLookup();
            var result = JSON.stringify(config);
            return result;
        }

        function SaveChannelOutputsJSON() {
            var configStr = GetChannelOutputConfig();
            $.post("api/configfile/channeloutputs.json",
                configStr
            ).done(function (data) {
                $.jGrowl(" Channel Output configuration saved", { themeState: 'success' });
                SetRestartFlag(1);
            }).fail(function () {
                DialogError("Save Channel Output Config", "Save Failed");
            });
        }

        function inputsAreSane() {
            var result = 1;

            result &= pixelStringInputsAreSane();

            return result;
        }

        function okToAddNewInput(type) {
            var result = 1;

            result &= okToAddNewPixelStringInput(type);

            return result;
        }


        /////////////////////////////////////////////////////////////////////////////

        <?

        $channelOutputs = array();
        $channelOutputsJSON = "";
        if (file_exists($settings['channelOutputsJSON'])) {
            $channelOutputsJSON = file_get_contents($settings['channelOutputsJSON']);
            $channelOutputs = json_decode($channelOutputsJSON, true);
            $channelOutputsJSON = preg_replace("/\n|\r/", "", $channelOutputsJSON);
            $channelOutputsJSON = preg_replace("/\"/", "\\\"", $channelOutputsJSON);
        }

        // If led panels are enabled, make sure the page is displayed even if the cape is a string cape (could be a colorlight output)
        if ($channelOutputs != null && $channelOutputs['channelOutputs'] != null && $channelOutputs['channelOutputs'][0] != null) {
            if ($channelOutputs['channelOutputs'][0]['type'] == "LEDPanelMatrix" && $channelOutputs['channelOutputs'][0]['enabled'] == 1) {
                $currentCapeInfo["provides"][] = 'panels';
            }
        }

        ?>

        function handleCOKeypress(e) {
            if (e.keyCode == 113) {
                if ($('.nav-link.active').attr('tabtype') == 'strings') {
                    setPixelStringsStartChannelOnNextRow();
                }
            }
        }


        function pageSpecific_PageLoad_DOM_Setup() {

            var channelOutputsJSON = "<? echo $channelOutputsJSON; ?>";

            if (channelOutputsJSON != "")
                channelOutputs = jQuery.parseJSON(channelOutputsJSON);
            else
                channelOutputs.channelOutputs = [];

            UpdateChannelOutputLookup();

        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            $(document).on('keydown', handleCOKeypress);

        }



    </script>
    <title><? echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'input-output';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class='title'>Channel Outputs</h1>
            <div class="pageContent">
                <div id='channelOutputManager' class="ui-corner-all ui-widget ui-widget-content">

                    <?
                    $lpTabStyle = " hidden";
                    $stringTabStyle = " hidden";
                    $e131TabStyle = " hidden";
                    $e131TabStyleActive = "";
                    $lpTabStyleActive = "";
                    $stringTabStyleActive = "";

                    if (in_array('all', $currentCapeInfo["provides"])) {
                        $lpTabStyle = "";
                        $e131TabStyle = "";
                        $stringTabStyle = "";
                    } else {
                        if (in_array('udp', $currentCapeInfo["provides"])) {
                            $e131TabStyle = "";
                            $lpTabStyle = "";
                        }
                        if (in_array('strings', $currentCapeInfo["provides"])) {
                            $stringTabStyle = "";
                        }
                        if (
                            in_array('panels', $currentCapeInfo["provides"])
                            || !in_array('strings', $currentCapeInfo["provides"])
                        ) {
                            $lpTabStyle = "";
                        }
                        if (
                            !in_array('panels', $currentCapeInfo["provides"])
                            && !in_array('strings', $currentCapeInfo["provides"])
                        ) {
                            $e131TabStyle = "";
                        }
                    }
                    if ($e131TabStyle == "") {
                        $e131TabStyleActive = "active";
                    } else if ($stringTabStyle == "") {
                        $stringTabStyleActive = "active";
                    } else if ($lpTabStyle == "") {
                        $lpTabStyleActive = "active";
                    }
                    ?>
                    <ul class="nav nav-pills pageContent-tabs" id="channelOutputTabs" role="tablist">
                        <li class="nav-item <?= $e131TabStyle ?>" id="tab-e131-LI" role="presentation">
                            <a class="nav-link <?= $e131TabStyleActive ?>" id="tab-e131-tab" type="button" tabType='UDP'
                                data-bs-toggle="pill" data-bs-target="#tab-e131" role="tab" aria-controls="tab-e131"
                                aria-selected="true">
                                E1.31 / ArtNet / DDP / KiNet
                            </a>
                        </li>

                        <?

                        if (
                            $settings['BeaglePlatform'] || $settings['Platform'] == "Raspberry Pi" ||
                            ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux"))
                        ) {
                            $stringTabText = "Pixel Strings";
                            if (in_array('all', $currentCapeInfo["provides"]) || in_array('strings', $currentCapeInfo["provides"])) {
                                if (isset($currentCapeInfo["labels"]) && isset($currentCapeInfo["labels"]["strings"])) {
                                    $stringTabText = $currentCapeInfo["labels"]["strings"];
                                } else if (
                                    ((in_array('all', $currentCapeInfo["provides"])) || (in_array('strings', $currentCapeInfo["provides"]))) &&
                                    (isset($currentCapeInfo["name"]) && $currentCapeInfo["name"] != "Unknown")
                                ) {
                                    $stringTabText = $currentCapeInfo["name"];
                                    if (in_array('all', $currentCapeInfo["provides"]) || in_array('panels', $currentCapeInfo["provides"])) {
                                        $stringTabText .= " Pixel Strings";
                                    }
                                }
                            }
                            ?>
                            <li class="nav-item <?= $stringTabStyle ?>" id="tab-strings-LI">
                                <a class="nav-link <?= $stringTabStyleActive ?>" id="stringTab-tab" type="button"
                                    tabType='strings' data-bs-toggle="pill" data-bs-target='#stringTab' role="tab"
                                    aria-controls="stringTab">
                                    <? echo $stringTabText; ?>
                                </a>
                            </li>
                            <?
                        }
                        if ($settings['Platform'] == "Raspberry Pi") {
                            if (in_array('fpd', $currentCapeInfo["provides"])) {
                                ?>
                                <li class="nav-item">
                                    <a class="nav-link" id="tab-fpd-tab" type="button" tabType='FPD' data-bs-toggle="pill"
                                        data-bs-target='#tab-fpd' role="tab" aria-controls="tab-fpd">
                                        Falcon Pixelnet/DMX
                                    </a>
                                </li>
                                <?
                            }
                        }
                        if (in_array('pwm', $currentCapeInfo["provides"]) && isset($currentCapeInfo['verifiedKeyId'])) {
                            $pwmTabText = "PWM";
                            if (isset($currentCapeInfo["labels"]) && isset($currentCapeInfo["labels"]["pwm"])) {
                                $pwmTabText = $currentCapeInfo["labels"]["pwm"];
                            }
                            ?>
                            <li class="nav-item">
                                <a class="nav-link" id="tab-pwm-tab" type="button" tabType='PWM' data-bs-toggle="pill"
                                    data-bs-target='#tab-pwm' role="tab" aria-controls="tab-pwm">
                                    <? echo $pwmTabText; ?>
                                </a>
                            </li>
                            <?
                        }

                        $ledTabText = "LED Panels";
                        if (
                            ((in_array('all', $currentCapeInfo["provides"])) || (in_array('panels', $currentCapeInfo["provides"]))) &&
                            (isset($currentCapeInfo["name"]) && $currentCapeInfo["name"] != "Unknown")
                        ) {
                            $ledTabText = $currentCapeInfo["name"];
                            if (in_array('all', $currentCapeInfo["provides"]) || in_array('strings', $currentCapeInfo["provides"])) {
                                $ledTabText .= " LED Panels";
                            }

                        }
                        ?>
                        <li class="nav-item <?= $lpTabStyle ?>" id="tab-LEDPanels-LI">
                            <a class="nav-link <?= $lpTabStyleActive ?>" id="tab-LEDPanels-tab" type="button"
                                tabType='panels' data-bs-toggle="pill" data-bs-target='#tab-LEDPanels' role="tab"
                                aria-controls="tab-LEDPanels">
                                <? echo $ledTabText; ?>
                            </a>
                        </li>
                        <li class="nav-item" id="tab-other-LI">
                            <a class="nav-link" id="tab-other-tab" type="button" tabType='other' data-bs-toggle="pill"
                                data-bs-target='#tab-other' role="tab" aria-controls="tab-other">
                                Other
                            </a>
                        </li>
                    </ul>


                    <!-- --------------------------------------------------------------------- -->
                    <?
                    if ($e131TabStyle == "") {
                        $e131TabStyleActive = "show active";
                    } else if ($stringTabStyle == "") {
                        $stringTabStyleActive = "show active";
                    } else if ($lpTabStyle == "") {
                        $lpTabStyleActive = "show active";
                    }
                    ?>

                    <div id="channelOutputTabsContent" class="tab-content">
                        <div class="tab-pane fade <?= $e131TabStyleActive ?>" id="tab-e131" role="tabpanel"
                            aria-labelledby="tab-e131-tab">
                            <? include_once 'co-universes.php'; ?>
                        </div>

                        <?

                        if (
                            $settings['BeaglePlatform'] || $settings['Platform'] == "Raspberry Pi" ||
                            ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux"))
                        ) {
                            ?>
                            <div class="tab-pane fade <?= $stringTabStyleActive ?>" id="stringTab" role="tabpanel"
                                aria-labelledby="stringTab-tab">
                                <? include_once 'co-pixelStrings.php'; ?>
                            </div>
                            <?
                        }
                        if ($settings['Platform'] == "Raspberry Pi") {
                            if (in_array('fpd', $currentCapeInfo["provides"])) {
                                ?>
                                <div class="tab-pane fade" id="tab-fpd" role="tabpanel" aria-labelledby="tab-fpd-tab">
                                    <? include_once 'co-fpd.php'; ?>
                                </div>
                                <?
                            }
                        }
                        if (in_array('pwm', $currentCapeInfo["provides"])) {
                            ?>
                            <div class="tab-pane fade" id="tab-pwm" role="tabpanel" aria-labelledby="tab-pwm-tab">
                                <? include_once 'co-pwm.php'; ?>
                            </div>
                            <?
                        }
                        ?>

                        <div class="tab-pane fade <?= $lpTabStyleActive ?>" id="tab-LEDPanels" role="tabpanel"
                            aria-labelledby="tab-LEDPanels-tab">
                            <? include_once 'co-ledPanels.php'; ?>
                        </div>
                        <div class="tab-pane fade" id="tab-other" role="tabpanel" aria-labelledby="tab-other-tab">
                            <? include_once "co-other.php"; ?>
                        </div>

                    </div> <!-- tabcontent close -->

                    <!-- --------------------------------------------------------------------- -->


                </div>



            </div> <!-- page content -->

            <div id='debugOutput'>
            </div>

        </div>

        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>