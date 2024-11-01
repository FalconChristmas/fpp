<script src='js/fabric.min.js'></script>
<script>

    <?
    $panelCapes = array();
    $panelCapes = readPanelCapes($mediaDirectory . "/tmp/panels/", $panelCapes);
    $panelCapesHaveSel4 = false;
    if (count($panelCapes) == 1) {
        echo "var KNOWN_PANEL_CAPE = " . $panelCapes[0] . ";";
        $panelCapes[0] = json_decode($panelCapes[0], true);
        if (isset($panelCapes[0]["controls"]["sel4"])) {
            $panelCapesHaveSel4 = true;
        }
    } else {
        echo "// NO KNOWN_PANEL_CAPE";
    }

    //set default values
    $LEDPanelOutputs = 24;
    $LEDPanelPanelsPerOutput = 24;
    $LEDPanelCols = array();
    $LEDPanelRows = array();
    $LEDPanelWidth = 32;
    $LEDPanelHeight = 16;
    $LEDPanelScan = 8;
    $LEDPanelAddressing = 0;
    $LEDPanelGamma = 2.2;

    if ($settings['BeaglePlatform']) {
        $LEDPanelPanelsPerOutput = 16;
        if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) {
            $LEDPanelOutputs = 5;
        } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
            $LEDPanelOutputs = 6;
        } else {
            $LEDPanelOutputs = 8;
        }
    }

    $maxLEDPanels = $LEDPanelOutputs * $LEDPanelPanelsPerOutput;
    $maxLEDPanels = 96; // Override to allow different panel configs using ColorLight cards
    
    // echo $settings;
    
    if (isset(($settings["LEDPanelMatrices"]))) {
        $matricesArray = json_decode($settings["LEDPanelMatrices"], true);
        for ($z = 1; $z <= count($matricesArray["panelMatrices"]); $z++) {
            $panelMatrixID = $matricesArray["panelMatrices"]["panelMatrix$z"]["panelMatrixID"];
            $parts = explode('x', $matricesArray["panelMatrices"]["panelMatrix$z"]["Configuration"]["LEDPanelsLayout"]);
            if (count($parts) == 2) {
                $LEDPanelCols[$panelMatrixID] = $parts[0];
                $LEDPanelRows[$panelMatrixID] = $parts[1];
            }
        }
    } else {
        //set some defaults for the first panelMatrix
        $LEDPanelCols[1] = 1;
        $LEDPanelRows[1] = 1;
    }

    function readPanelCapes($cd, $panelCapes)
    {
        if (is_dir($cd)) {
            if ($dh = opendir($cd)) {
                while (($file = readdir($dh)) !== false) {
                    $string = "";
                    if (substr($file, 0, 1) == '.') {
                        $string = "";
                    } else {
                        $string = file_get_contents($cd . $file);
                    }
                    if ($string != "") {
                        $panelCapes[] = $string;
                    }
                }
                closedir($dh);
            }
        }
        return $panelCapes;
    }
    function printLEDPanelLayoutSelect($panelMatrixID)
    {
        global $maxLEDPanels, $LEDPanelCols, $LEDPanelRows;

        echo "W: <select class='LEDPanelsLayoutCols' onChange='LEDPanelsLayoutChanged()'>\n";
        for ($r = 1; $r <= $maxLEDPanels; $r++) {
            if ($LEDPanelCols[$panelMatrixID] == $r) {
                echo "<option value='" . $r . "' selected>" . $r . "</option>\n";
            } else {
                echo "<option value='" . $r . "'>" . $r . "</option>\n";
            }
        }
        echo "</select>";

        echo "&nbsp; H:<select class='LEDPanelsLayoutRows' onChange='LEDPanelsLayoutChanged()'>\n";
        for ($r = 1; $r <= $maxLEDPanels; $r++) {
            if ($LEDPanelRows[$panelMatrixID] == $r) {
                echo "<option value='" . $r . "' selected>" . $r . "</option>\n";
            } else {
                echo "<option value='" . $r . "'>" . $r . "</option>\n";
            }
        }
        echo "</select>";
    }

    function printLEDPanelGammaSelect($platform, $gamma)
    {
        if (!isset($gamma) || $gamma == "" || $gamma == "0") {
            $gamma = "2.2";
        }
        echo "<input type='number' min='0.1' max='5.0' step='0.1' value='$gamma' class='LEDPanelsGamma'>";
    }

    function printLEDPanelInterleaveSelect()
    {
        global $settings;
        //get currently visible panelMatrixID
        //echo "panelMatrixID = $('.panelMatrix-tab-content .tab-pane.active .divPanelMatrixID')[0].innerHTML;\n";
        echo "<select class='LEDPanelInterleave' onchange=\"LEDPanelLayoutChanged();\">";

        $values = array();
        if ($settings['BeaglePlatform']) {
            $values["Off"] = "0";
            $values["4 Pixels"] = "4";
            $values["8 Pixels"] = "8";
            $values["16 Pixels"] = "16";
            $values["32 Pixels"] = "32";
            $values["64 Pixels"] = "64";
            $values["80 Pixels"] = "80";
            $values["4 Pixels Zig/Zag"] = "4z";
            $values["8 Pixels Zig/Zag"] = "8z";
            $values["16 Pixels Zig/Zag"] = "16z";
            $values["8 Pixels Flip Rows"] = "8f";
            $values["16 Pixels Flip Rows"] = "16f";
            $values["32 Pixels Flip Rows"] = "32f";
            $values["64 Pixels Flip Rows"] = "64f";
            $values["80 Pixels Flip Rows"] = "80f";
            $values["8 Pixels Cluster Zig/Zag"] = "8c";
            $values["8 Stripe/16 Cluster"] = "8s";
        } else {
            $values["Off"] = "0";
            $values["Stripe"] = "1";
            $values["Checkered"] = "2";
            $values["Spiral"] = "3";
            $values["ZStripe"] = "4";
            $values["ZnMirrorZStripe"] = "5";
            $values["Coreman"] = "6";
            $values["Kaler2Scan"] = "7";
            $values["ZStripeUneven"] = "8";
            $values["P10-128x4-Z"] = "9";
            $values["QiangLiQ8"] = "10";
            $values["InversedZStripe"] = "11";
            $values["P10Outdoor1R1G1-1"] = "12";
            $values["P10Outdoor1R1G1-2"] = "13";
            $values["P10Outdoor1R1G1-3"] = "14";
            $values["P10CoremanMapper"] = "15";
            $values["P8Outdoor1R1G1"] = "16";
            $values["FlippedStripe"] = "17";
            $values["P10Outdoor32x16HalfScan"] = "18";
            $values["P10Outdoor32x16QuarterScanMapper"] = "19";
            $values["P3Outdoor64x64MultiplexMapper"] = "20";
        }
        foreach ($values as $key => $value) {
            echo "<option value='$value'";
            if ($value == "0") {
                echo " selected";
            }

            echo ">$key</option>\n";
        }
        echo "</select>";
    }

    ?>
    //END OF PHP ---BEGINNING OF JAVASCRIPT FUNCTIONS

    var LEDPanelColorOrder = 'RGB';
    var LEDPanelOutputs = <? echo $LEDPanelOutputs; ?>;
    var LEDPanelPanelsPerOutput = <? echo $LEDPanelPanelsPerOutput; ?>;
    var LEDPanelWidth = <? echo $LEDPanelWidth; ?>;
    var LEDPanelHeight = <? echo $LEDPanelHeight; ?>;
    var LEDPanelScan = <? echo $LEDPanelScan; ?>;
    var LEDPanelAddressing = <? echo $LEDPanelAddressing; ?>;
    const LEDPanelRows = [];
    const LEDPanelCols = [];
    <? for ($mp = 0; $mp <= 5; $mp++) {
        if (isset($LEDPanelRows[$mp]) && isset($LEDPanelCols[$mp])) {
            echo "LEDPanelRows[" . $mp . "] = " . $LEDPanelRows[$mp] . ";\n";
            echo "LEDPanelCols[" . $mp . "] = " . $LEDPanelCols[$mp] . ";\n";
        }

    }
    ?>

    LEDPanelRows[1] = <? echo $LEDPanelRows[1]; ?>;
    LEDPanelCols[1] = <? echo $LEDPanelCols[1]; ?>;
    var LEDPanelGamma = <? echo $LEDPanelGamma; ?>;

    function UpdatePanelSize() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        var size = $(`#panelMatrix${panelMatrixID} .LEDPanelsSize`).val();
        var sizeparts = size.split("x");
        LEDPanelWidth = parseInt(sizeparts[0]);
        LEDPanelHeight = parseInt(sizeparts[1]);
        LEDPanelScan = parseInt(sizeparts[2]);
        LEDPanelAddressing = parseInt(sizeparts[3]);
    }

    function LEDPanelOrientationClicked(id) {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        var src = $(`#panelMatrix${panelMatrixID} .${id}`).attr('src');

        if (src == 'images/arrow_N.png')
            $(`#panelMatrix${panelMatrixID} .${id}`).attr('src', 'images/arrow_R.png');
        else if (src == 'images/arrow_R.png')
            $(`#panelMatrix${panelMatrixID} .${id}`).attr('src', 'images/arrow_U.png');
        else if (src == 'images/arrow_U.png')
            $(`#panelMatrix${panelMatrixID} .${id}`).attr('src', 'images/arrow_L.png');
        else if (src == 'images/arrow_L.png')
            $(`#panelMatrix${panelMatrixID} .${id}`).attr('src', 'images/arrow_N.png');
    }

    function GetLEDPanelNumberSetting(id, key, maxItems, selectedItem) {
        var html = "";
        var i = 0;
        var o = 0;
        var selected = "";

        html = "<select class='" + key + "'>";
        for (i = 0; i < maxItems; i++) {
            selected = "";

            if (i == selectedItem)
                selected = "selected";

            html += "<option value='" + i + "' " + selected + ">" + id + "-" + (i + 1) + "</option>";
        }
        html += "</select>";

        return html;
    }

    function GetLEDPanelColorOrder(key, selectedItem) {
        var colorOrders = ["RGB", "RBG", "GBR", "GRB", "BGR", "BRG"];
        var html = "";
        var selected = "";
        var i = 0;

        html += "<select class='" + key + "'>";
        html += "<option value=''>C-Def</option>";
        for (i = 0; i < colorOrders.length; i++) {
            selected = "";

            if (colorOrders[i] == selectedItem)
                selected = "selected";

            html += "<option value='" + colorOrders[i] + "' " + selected + ">C-" + colorOrders[i] + "</option>";
        }
        html += "</select>";

        return html;
    }

    function UpdateLegacyLEDPanelLayout(panelMatrixID) {
        LEDPanelCols[panelMatrixID] = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutCols`).val());
        LEDPanelRows[panelMatrixID] = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutRows`).val());

        UpdatePanelSize();
        if ((LEDPanelScan * 2) == LEDPanelHeight) {
            $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).attr("disabled", "disabled");
        } else {
            $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).removeAttr("disabled");
        }

        var pixelCount = LEDPanelCols[panelMatrixID] * LEDPanelRows[panelMatrixID] * LEDPanelWidth * LEDPanelHeight;
        $(`#panelMatrix${panelMatrixID} .LEDPanelsPixelCount`).html(pixelCount);

        var channelCount = pixelCount * 3;
        $(`#panelMatrix${panelMatrixID} .LEDPanelsChannelCount`).html(channelCount);

        DrawLEDPanelTable(panelMatrixID);
    }

    function LEDPanelLayoutChanged() {
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        UpdateLegacyLEDPanelLayout(panelMatrixID);

        // update in-memory array
        GetChannelOutputConfig();

        if ($(`#panelMatrix${panelMatrixID} .LEDPanelUIAdvancedLayout`).is(":checked")) {
            for (var i = 0; i < channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels.length; i++) {
                SetupCanvasPanel(panelMatrixID, i);
            }
            UpdateMatrixSize(panelMatrixID);
        }
    }

    function FindIndexOfPanelMatricesArrayBasedOnPanelID(panelMatrixID) {
        var index = settings["LEDPanelMatrices"]["panelMatrices"].findIndex(function (match) {
            return match.panelMatrixID == panelMatrixID
        });
        return index;
    }

    function LEDPanelsLayoutChanged() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        // MatrixArrayIndex = FindIndexOfPanelMatricesArrayBasedOnPanelID(panelMatrixID);

        var PanelLayoutValue = $(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutCols`).val() + "x" + $(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutRows`).val();

        var PanelLayoutUpdateJSONObj = {};
        PanelLayoutUpdateJSONObj.panelMatrices = {};
        PanelLayoutUpdateJSONObj.panelMatrices.panelMatrix1 = {};
        PanelLayoutUpdateJSONObj.panelMatrices.panelMatrix1.Configuration = {};
        PanelLayoutUpdateJSONObj.panelMatrices.panelMatrix1.Configuration.LEDPanelsLayout = {};

        PanelLayoutUpdateJSONObj.panelMatrices.panelMatrix1.Configuration.LEDPanelsLayout = PanelLayoutValue;

        var PanelLayoutValueJSON = JSON.stringify(PanelLayoutUpdateJSONObj);

        $.put('api/settings/LEDPanelMatrices/jsonValueUpdate', PanelLayoutValueJSON)
            .done(function () {
                $.jGrowl('Panel Layout saved', { themeState: 'success' });
                settings["LEDPanelMatrices"]["panelMatrices"]["panelMatrix" + panelMatrixID]["Configuration"]["LEDPanelsLayout"] = PanelLayoutValue;
                LEDPanelLayoutChanged();
                SetRestartFlag(2);
                common_ViewPortChange();
            }).fail(function () {
                DialogError('Panel Layout', 'Failed to save Panel Layout');
            });
    }


    function FrontBackViewToggled() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        GetChannelOutputConfig(); // Refresh the in-memory config before redrawing
        DrawLEDPanelTable(panelMatrixID);
    }

    function DrawLEDPanelTable(panelMatrixID) {
        var r;
        var c;
        var key = "";
        var frontView = 0;

        var tbody = $(`#panelMatrix${panelMatrixID} .LEDPanelTable tbody`);
        tbody.empty();
        if ($(`#panelMatrix${panelMatrixID} .LEDPanelUIFrontView`).is(":checked")) {
            frontView = 1;
            tbody[0].insertRow(0).innerHTML = "<th colspan='" + LEDPanelCols[panelMatrixID] + "'>Front View</th>";
        } else {
            tbody[0].insertRow(0).innerHTML = "<th colspan='" + LEDPanelCols[panelMatrixID] + "'>Back View</th>";
        }

        for (r = 0; r < LEDPanelRows[panelMatrixID]; r++) {
            var html = "<tr>";
            for (i = 0; i < LEDPanelCols[panelMatrixID]; i++) {
                if (frontView)
                    c = i;
                else
                    c = LEDPanelCols[panelMatrixID] - 1 - i;

                html += "<td><table cellspacing=0 cellpadding=0><tr><td>";

                key = "LEDPanelOutputNumber_" + r + "_" + c;
                if (typeof channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key] !== 'undefined') {
                    html += GetLEDPanelNumberSetting("O", key, LEDPanelOutputs, channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key].outputNumber);
                } else {
                    html += GetLEDPanelNumberSetting("O", key, LEDPanelOutputs, 0);
                }

                html += "<img src='images/arrow_";

                if (typeof channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key] !== 'undefined') {
                    html += channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key].orientation;
                } else {
                    html += "N";
                }

                html += ".png' height=17 width=17 class='LEDPanelOrientation_" + r + "_" + c + "' onClick='LEDPanelOrientationClicked(\"LEDPanelOrientation_" + r + "_" + c + "\");'><br>";

                key = "LEDPanelPanelNumber_" + r + "_" + c;
                if (typeof channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key] !== 'undefined') {
                    html += GetLEDPanelNumberSetting("P", key, LEDPanelPanelsPerOutput, channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key].panelNumber);
                } else {
                    html += GetLEDPanelNumberSetting("P", key, LEDPanelPanelsPerOutput, 0);
                }
                html += "<br>";

                key = "LEDPanelColorOrder_" + r + "_" + c;
                if (typeof channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key] !== 'undefined') {
                    html += GetLEDPanelColorOrder(key, channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID][key].colorOrder);
                } else {
                    html += GetLEDPanelColorOrder(key, "");
                }

                html += "</td></tr></table></td>\n";
            }
            html += "</tr>";
            tbody[0].insertRow(-1).innerHTML = html;
        }
    }

    function InitializeLEDPanelMatrix(panelMatrixID) {
        if (typeof (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID]) !== "object") {
            return;
        }

        if ((Object.values(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID]).includes("LEDPanelMatrix")) &&
            (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].enabled == 1)) {
            $(`#panelMatrix${panelMatrixID} .LEDPanelsEnabled`).prop('checked', true);
            $(`#panelMatrix${panelMatrixID} .tab-LEDPanels-LI`).show();
        }

        if (Object.values(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID]).includes("LEDPanelMatrix")) {
            $(`#panelMatrix${panelMatrixID} .LEDPanelsStartChannel`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].startChannel);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsPixelCount`).html(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].channelCount / 3);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsChannelCount`).html(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].channelCount);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorOrder`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].colorOrder);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightness`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].brightness);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsGamma`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].gamma);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].subType);
            if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].interface != null) {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].interface);
            }
            <?
            if ($settings['Platform'] == "Raspberry Pi" || $settings['BeaglePlatform']) {
                ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].wiringPinout);
                <?
            }

            if ($settings['Platform'] == "Raspberry Pi") {
                ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].gpioSlowdown);
                <?
            }
            ?>
            var outputByRow = false;
            var outputBlank = false;
            if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelOutputOrder != null) {
                outputByRow = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelOutputOrder;
            }
            var colordepth = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelColorDepth;
            if (typeof colordepth === 'undefined') {
                colordepth = 8;
            }
            if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelRowAddressType != null) {
                var RowAddressType = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelRowAddressType;
                $(`#panelMatrix${panelMatrixID} .LEDPanelRowAddressType`).val(RowAddressType);
            }
            if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelInterleave != null) {
                var interleave = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelInterleave;
                $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).val(interleave);
            }

            <? if ($settings['BeaglePlatform']) { ?>
                if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelOutputBlankRow != null) {
                    outputBlank = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelOutputBlankRow;
                }
                if (colordepth < 0) {
                    outputBlank = true;
                    colordepth = -colordepth;
                    outputByRow = true;
                }
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).prop("checked", outputByRow);
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).prop("checked", outputBlank);
                if (outputByRow == false) {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankLabel`).hide();
                }
            <? } else if ($settings['Platform'] == "Raspberry Pi") { ?>
                    var cpuPWM = false;
                    var cpuPWM = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].cpuPWM;
                    if (typeof cpuPWM === 'undefined') {
                        cpuPWM = false;
                    }
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWM`).prop("checked", cpuPWM);
            <? } ?>

            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).val(colordepth);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsStartCorner`).val(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].invertedData);

            if ((channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].subType == 'ColorLight5a75') ||
                (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].subType == 'X11PanelMatrix')) {
                LEDPanelOutputs = 24;
                LEDPanelPanelsPerOutput = 24;
            }
        }

        <?
        if ($settings['Platform'] == "Raspberry Pi" || $settings['BeaglePlatform']) {
            ?>

            if (typeof KNOWN_PANEL_CAPE !== 'undefined') {
                if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"] !== 'undefined') {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"]);
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinoutLabel`).hide();
                }
                if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"] !== 'undefined') {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionType`).innerText = KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"];
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionType`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionLabel`).hide();
                }
                LEDPanelOutputs = KNOWN_PANEL_CAPE["outputs"].length;
            }
            <?
        }
        ?>

        PanelSubtypeChanged(panelMatrixID);
        UpdateLegacyLEDPanelLayout(panelMatrixID);
    }



    function GetLEDPanelConfigFromUI(panelMatrixID) {
        var config = new Object();
        var panels = "";
        var xOffset = 0;
        var yOffset = 0;
        var yDiff = 0;
        var advanced = 0;

        var matrixDivName = 'panelMatrix' + panelMatrixID;
        var matrixDiv = $(`.tab-content [id=${matrixDivName}]`);

        if ((matrixDiv.find(`#panelMatrix${panelMatrixID} .LEDPanelUIAdvancedLayout`).is(":checked")) &&
            (typeof channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID] !== 'undefined'))
            advanced = 1;
        config.type = "LEDPanelMatrix";
        <?
        if ($settings['BeaglePlatform']) {
            echo "config.subType = 'LEDscapeMatrix';\n";
        } else {
            echo "config.subType = 'RGBMatrix';\n";
        }
        ?>

        UpdatePanelSize();

        config.panelMatrixID = panelMatrixID;
        config.enabled = 0;
        config.cfgVersion = 2;
        config.startChannel = parseInt(matrixDiv.find('.LEDPanelsStartChannel').val());
        config.channelCount = parseInt(matrixDiv.find('.LEDPanelsChannelCount').html());
        config.colorOrder = matrixDiv.find('.LEDPanelsColorOrder').val();
        config.gamma = matrixDiv.find('.LEDPanelsGamma').val();
        if ((matrixDiv.find('.LEDPanelsConnectionSelect')[0].value === "ColorLight5a75") || (matrixDiv.find('.LEDPanelsConnectionSelect')[0].value === "X11PanelMatrix")) {
            config.subType = matrixDiv.find('.LEDPanelsConnectionSelect')[0].value;
            if (matrixDiv.find('.LEDPanelsConnectionSelect')[0].value != "X11PanelMatrix")
                config.interface = matrixDiv.find('.LEDPanelsInterface').val();

        }
        <?
        if ($settings['Platform'] == "Raspberry Pi" || $settings['BeaglePlatform']) {
            ?>
            config.wiringPinout = matrixDiv.find('.LEDPanelsWiringPinout').val();
            config.brightness = parseInt(matrixDiv.find('.LEDPanelsBrightness').val());
            <?
        }

        if ($settings['Platform'] == "Raspberry Pi") {
            ?>
            config.gpioSlowdown = parseInt(matrixDiv.find('.LEDPanelsGPIOSlowdown').val());
            <?
        }
        ?>

        config.panelColorDepth = parseInt(matrixDiv.find('.LEDPanelsColorDepth').val());
        config.gamma = matrixDiv.find('.LEDPanelsGamma').val();
        config.invertedData = parseInt(matrixDiv.find('.LEDPanelsStartCorner').val());
        config.ledPanelsLayout = matrixDiv.find('.LEDPanelsLayoutCols').val() + "x" + matrixDiv.find('.LEDPanelsLayoutRows').val();
        config.panelWidth = LEDPanelWidth;
        config.panelHeight = LEDPanelHeight;
        config.panelScan = LEDPanelScan;
        <? if ($settings['Platform'] == "Raspberry Pi" || $settings['BeaglePlatform']) { ?>
            config.panelOutputOrder = matrixDiv.find('.LEDPanelsOutputByRow').is(':checked');
            config.panelOutputBlankRow = matrixDiv.find('.LEDPanelsOutputBlankRow').is(':checked');
        <? } ?>
        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
            config.cpuPWM = matrixDiv.find('.LEDPanelsOutputCPUPWM').is(':checked');
        <? } ?>

        if (LEDPanelAddressing) {
            config.panelAddressing = LEDPanelAddressing;
        }
        if (matrixDiv.find('.LEDPanelRowAddressType').val() != "0") {
            config.panelRowAddressType = parseInt(matrixDiv.find('.LEDPanelRowAddressType').val());
        }
        if (matrixDiv.find('.LEDPanelInterleave').val() != "0") {
            config.panelInterleave = matrixDiv.find('.LEDPanelInterleave').val();
        }
        config.panels = [];

        if (matrixDiv.find('.LEDPanelsEnabled').is(":checked"))
            config.enabled = 1;

        if (advanced) {
            config.panels = GetAdvancedPanelConfig(panelMatrixID);
        } else {
            for (r = 0; r < LEDPanelRows[panelMatrixID]; r++) {
                xOffset = 0;

                for (c = 0; c < LEDPanelCols[panelMatrixID]; c++) {
                    var panel = new Object();
                    var id = "";

                    id = ".LEDPanelOutputNumber_" + r + "_" + c;
                    panel.outputNumber = parseInt(matrixDiv.find(id).val());

                    id = ".LEDPanelPanelNumber_" + r + "_" + c;
                    panel.panelNumber = parseInt(matrixDiv.find(id).val());

                    id = ".LEDPanelColorOrder_" + r + "_" + c;
                    panel.colorOrder = matrixDiv.find(id).val();

                    id = ".LEDPanelOrientation_" + r + "_" + c;
                    var src = matrixDiv.find(id).attr('src');

                    panel.xOffset = xOffset;
                    panel.yOffset = yOffset;

                    if (src == 'images/arrow_N.png') {
                        panel.orientation = "N";
                        xOffset += LEDPanelWidth;
                        yDiff = LEDPanelHeight;
                    }
                    else if (src == 'images/arrow_R.png') {
                        panel.orientation = "R";
                        xOffset += LEDPanelHeight;
                        yDiff = LEDPanelWidth;
                    }
                    else if (src == 'images/arrow_U.png') {
                        panel.orientation = "U";
                        xOffset += LEDPanelWidth;
                        yDiff = LEDPanelHeight;
                    }
                    else if (src == 'images/arrow_L.png') {
                        panel.orientation = "L";
                        xOffset += LEDPanelHeight;
                        yDiff = LEDPanelWidth;
                    }

                    panel.row = r;
                    panel.col = c;

                    config.panels.push(panel);
                }
                yOffset += yDiff;
            }
        }

        return config;
    }


    <?
    function PopulateEthernetInterfaces()
    {
        $interfaces = network_list_interfaces_array();
        foreach ($interfaces as $iface) {
            $iface = preg_replace("/:$/", "", $iface);
            echo "<option value='" . $iface;
            echo "'";
            if ($iface === "eth0") {
                echo " selected";
            }

            $ifaceSpeed = (int) file_get_contents("/sys/class/net/$iface/speed");
            echo ">" . $iface . " (" . $ifaceSpeed . "Mbps)</option>";
        }
    }
    ?>

    function LEDPanelsConnectionChanged(panelMatrixID = GetCurrentActiveMatrixPanelID()) {

        WarnIfSlowNIC(panelMatrixID);

        if (($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "ColorLight5a75") || ($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "X11PanelMatrix")) {
            $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdownLabel`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightness`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightnessLabel`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepthLabel`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinoutLabel`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).hide();

            if ($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "X11PanelMatrix") {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionInterface`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).hide();
            } else {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionInterface`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).show();
            }

            <?
            //NEEDS FIXING FOR MULTI MATRIX
            if ($settings['BeaglePlatform']) { ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressTypeLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelRowAddressType`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsInterleaveLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRowLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRowLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).hide();
            <? } ?>


            <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsInterleaveLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWMLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWM`).hide();
            <? } ?>

            LEDPanelOutputs = 24;
        }
        else {
            $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionInterface`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightness`).show();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).show();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightnessLabel`).show();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepthLabel`).show();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinoutLabel`).show();
            $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).show();

            <? //NEEDS FIX
            if ($settings['BeaglePlatform']) { ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsInterleaveLabel`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRowLabel`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).show();
                var checked = $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).is(':checked');
                if (checked != false) {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRowLabel`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).show();
                } else {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRowLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).hide();
                }

                <?
                if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) {
                    echo "        LEDPanelOutputs = 5;\n";
                } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
                    echo "        LEDPanelOutputs = 6;\n";
                } else {
                    echo "        LEDPanelOutputs = 8;\n";
                }
                ?> //////
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdownLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).hide();
            <? //NEEDS FIX
            } else {
                if ($settings['Platform'] == "Raspberry Pi") { ?>
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressTypeLabel`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelRowAddressType`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsInterleaveLabel`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWMLabel`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWM`).show();
                <? }
                ?> /////
                LEDPanelOutputs = 3;
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdownLabel`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).show();
                <?
            }
            ?>
        }

        if (typeof KNOWN_PANEL_CAPE !== 'undefined') {
            if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"] !== 'undefined') {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsWiringPinout"]);
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinoutLabel`).hide();
            }
            if (KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"] !== 'undefined') {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`).text(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"]);
                $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionType`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionLabel`).hide();
            }
            LEDPanelOutputs = KNOWN_PANEL_CAPE["outputs"].length;
        }

        PanelSubtypeChanged(panelMatrixID);
        DrawLEDPanelTable(panelMatrixID);
    }

    <? if ($settings['BeaglePlatform']) { ?>
        function outputByRowClicked() {
            var checked = $('#LEDPanelsOutputByRow').is(':checked')
            if (checked != false) {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRowLabel`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).show();
            } else {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRowLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).hide();
            }
        }
    <? }
    ?>

    /////////////////////////////////
    var canvasInitialized = 0;
    var uiScale = 3;
    var panelGroups = [];
    const ledPanelCanvas = [];
    var selectedPanel = -1;
    var canvasWidth = 900;
    var canvasHeight = 400;

    function GetAdvancedPanelConfig(panelMatrixID) {
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var panels = [];

        for (var key in panelGroups[panelMatrixID]) {
            var panel = new Object();
            var pg = panelGroups[panelMatrixID][key];

            p = co.panels[pg.panelNumber];

            panel.outputNumber = p.outputNumber;
            panel.panelNumber = p.panelNumber;
            panel.xOffset = Math.round(pg.group.left / uiScale);
            panel.yOffset = Math.round(pg.group.top / uiScale);
            panel.orientation = p.orientation;
            panel.colorOrder = p.colorOrder;
            panel.row = p.row;
            panel.col = p.col;

            panels.push(panel);
        }

        return panels;
    }

    function snapToGrid(panelMatrixID, o) {
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var left = Math.round(o.left / uiScale);
        var top = Math.round(o.top / uiScale);
        var maxLeft = (canvasWidth / uiScale) - co.panelWidth;
        var maxTop = (canvasHeight / uiScale) - co.panelHeight;

        if ((o.orientation == 'R') ||
            (o.orientation == 'L')) {
            maxLeft = (canvasWidth / uiScale) - co.panelHeight;
            maxTop = (canvasHeight / uiScale) - co.panelWidth;
        }

        if (left < 0)
            left = 0;
        else if (left > maxLeft)
            left = maxLeft;

        if (top < 0)
            top = 0;
        else if (top > maxTop)
            top = maxTop;

        o.set({
            left: left * uiScale,
            top: top * uiScale
        });
    }

    function panelMovingHandler(evt) {
        panelMatrixID = GetCurrentActiveMatrixPanelID();

        var o = evt.target;
        var panels = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels;
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var name = o.name;
        var outputNumber = o.outputNumber;
        var panelNumber = o.panelNumber;
        var colorOrder = o.colorOrder;

        snapToGrid(o);

        var left = Math.round(o.left / uiScale);
        var top = Math.round(o.top / uiScale);
        var desc = 'O-' + (outputNumber + 1) + ' P-' + (panelNumber + 1) + '\n@ ' + left + ',' + top;
        panelGroups[panelMatrixID][name].text.set('text', desc);

        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelGroups[panelMatrixID][name].panelNumber].xOffset = left;
        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelGroups[panelMatrixID][name].panelNumber].yOffset = top;

        panelSelectedHandler(evt);
        ledPanelCanvas[panelMatrixID].renderAll();

        UpdateMatrixSize(panelMatrixID);
    }

    function panelSelectedHandler(evt) {
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        var o = ledPanelCanvas[panelMatrixID].getActiveObject();

        if (selectedPanel != -1)
            panelGroups[panelMatrixID]['panel-' + selectedPanel].panel.set({ 'stroke': 'black' });

        if (!o) {
            $(`#panelMatrix${panelMatrixID} .cpOutputNumber`).val(0);
            $(`#panelMatrix${panelMatrixID} .cpPanelNumber`).val(0);
            $(`#panelMatrix${panelMatrixID} .cpColorOrder`).val('');
            $(`#panelMatrix${panelMatrixID} .cpXOffset`).html('');
            $(`#panelMatrix${panelMatrixID} .cpYOffset`).html('');
            selectedPanel = -1;
        } else {
            $(`#panelMatrix${panelMatrixID} .cpOutputNumber`).val(o.outputNumber);
            $(`#panelMatrix${panelMatrixID} .cpPanelNumber`).val(o.panelNumber);
            $(`#panelMatrix${panelMatrixID} .cpColorOrder`).val(o.colorOrder);
            $(`#panelMatrix${panelMatrixID} .cpXOffset`).html(Math.round(o.left / uiScale));
            $(`#panelMatrix${panelMatrixID} .cpYOffset`).html(Math.round(o.top / uiScale));
            selectedPanel = panelGroups[panelMatrixID][o.name].panelNumber;
            panelGroups[panelMatrixID][o.name].panel.set({ 'stroke': 'rgb(255,255,0)' });
        }

        ledPanelCanvas[panelMatrixID].renderAll();
    }

    function RotateCanvasPanel() {
        panelMatrixID = GetCurrentActiveMatrixPanelID();

        var selection = ledPanelCanvas[panelMatrixID].getActiveObject();

        if (!selection) {
            return;
        } else if (selection.type === 'activeSelection') {
            // multiple items selected
        } else {
            var panelNumber = panelGroups[panelMatrixID][selection.name].panelNumber;

            channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].xOffset = Math.round(selection.left / uiScale);
            channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].yOffset = Math.round(selection.top / uiScale);

            if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation == 'N')
                channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation = 'R';
            else if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation == 'R')
                channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation = 'U';
            else if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation == 'U')
                channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation = 'L';
            else if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation == 'L')
                channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[panelNumber].orientation = 'N';

            SetupCanvasPanel(panelMatrixID, panelNumber);
        }

        UpdateMatrixSize(panelMatrixID);
    }

    function GetOutputNumberColor(output) {
        switch (output) {
            case 0: return 'rgb(231, 76, 60)';
            case 1: return 'rgb( 88,214,141)';
            case 2: return 'rgb( 93,173,226)';
            case 3: return 'rgb(244,208, 63)';
            case 4: return 'rgb(229,152,102)';
            case 5: return 'rgb(195,155,211)';
            case 6: return 'rgb(162,217,206)';
            case 7: return 'rgb(174,182,191)';
            case 8: return 'rgb(212,239,223)';
            case 9: return 'rgb(246,221,204)';
            case 10: return 'rgb(245,183,177)';
            case 11: return 'rgb(183,149, 11)';
        }

        return 'rgb(255,  0,  0)';
    }

    function SetupCanvasPanel(panelMatrixID, panelNumber) {
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var p = co.panels[panelNumber];
        var name = 'panel-' + panelNumber;

        if ((typeof panelGroups[panelMatrixID] !== 'undefined') && (name in panelGroups[panelMatrixID]))
            ledPanelCanvas[panelMatrixID].remove(panelGroups[panelMatrixID][name].group);

        var w = co.panelWidth * uiScale;
        var h = co.panelHeight * uiScale;
        var a = 0;
        var tol = 0;
        var tot = 0;
        var aw = 9;
        var ah = 14;

        if ((p.orientation == 'R') ||
            (p.orientation == 'L')) {
            w = co.panelHeight * uiScale;
            h = co.panelWidth * uiScale;

            if (p.orientation == 'R') {
                a = 90;
                tot = 0;
                tol = ah;
            } else {
                a = 270;
                tot = aw;
                tol = 0;
            }
        } else if (p.orientation == 'U') {
            a = 180;
            tot = ah;
            tol = aw;
        }

        var panel = new fabric.Rect({
            fill: GetOutputNumberColor(p.outputNumber),
            originX: 'center',
            originY: 'center',
            width: w - 1,
            height: h - 1,
            stroke: 'black',
            strokeWidth: 1
        });

        var desc = 'O-' + (p.outputNumber + 1) + ' P-' + (p.panelNumber + 1) + '\n@ ' + p.xOffset + ',' + p.yOffset;
        var text = new fabric.Text(desc, {
            fontSize: 12,
            originX: 'center',
            originY: 'center'
        });

        var arrow = new fabric.Triangle({
            width: aw,
            height: ah,
            fill: 'black',
            left: tol - parseInt(w / 2 - 3),
            top: tot - parseInt(h / 2 - 3),
            angle: a
        });

        var group = new fabric.Group([panel, text, arrow], {
            left: p.xOffset * uiScale,
            top: p.yOffset * uiScale,
            borderColor: 'black',
            angle: 0,
            hasControls: false
        });

        group.toObject = (function (toObject) {
            return function () {
                return fabric.util.object.extend(toObject.call(this), {
                    name: this.name,
                    outputNumber: this.outputNumber,
                    panelNumber: this.panelNumber,
                    colorOrder: this.colorOrder,
                    orientation: this.orientation
                });
            };
        })(group.toObject);

        group.name = name;
        group.outputNumber = p.outputNumber;
        group.panelNumber = p.panelNumber;
        group.colorOrder = p.colorOrder;
        group.orientation = p.orientation;

        var pg = {};
        pg.panelNumber = panelNumber;
        pg.group = group;
        pg.panel = panel;
        pg.text = text;
        pg.arrow = arrow;
        if (typeof panelGroups[panelMatrixID] === 'undefined') panelGroups[panelMatrixID] = [];
        panelGroups[panelMatrixID][name] = pg;

        ledPanelCanvas[panelMatrixID].add(group);
        ledPanelCanvas[panelMatrixID].setActiveObject(group);
        panelSelectedHandler();
    }

    function UpdateMatrixSize(panelMatrixID) {
        var matrixWidth = 0;
        var matrixHeight = 0;

        for (var key in panelGroups[panelMatrixID]) {
            var panel = new Object();
            var pg = panelGroups[panelMatrixID][key];

            if ((channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[pg.panelNumber].orientation == 'N') ||
                (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[pg.panelNumber].orientation == 'U')) {
                if ((Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelWidth) > matrixWidth)
                    matrixWidth = Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelWidth;
                if ((Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelHeight) > matrixHeight)
                    matrixHeight = Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelHeight;
            } else {
                if ((Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelHeight) > matrixWidth)
                    matrixWidth = Math.round(pg.group.left / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelHeight;
                if ((Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelWidth) > matrixHeight)
                    matrixHeight = Math.round(pg.group.top / uiScale) + channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelWidth;
            }
        }

        var matrixPixels = matrixWidth * matrixHeight;
        var matrixChannels = matrixPixels * 3;

        $(`#panelMatrix${panelMatrixID} .matrixSize`).html('' + matrixWidth + 'x' + matrixHeight);
        $(`#panelMatrix${panelMatrixID} .LEDPanelsChannelCount`).html(matrixChannels);
        $(`#panelMatrix${panelMatrixID} .LEDPanelsPixelCount`).html(matrixPixels);

        var resized = 0;
        if (matrixWidth > parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsWide`).val())) {
            resized = 1;
            $(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsWide`).val(matrixWidth);
        }
        if (matrixHeight > parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsHigh`).val())) {
            resized = 1;
            $(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsHigh`).val(matrixHeight);
        }
        if (resized)
            SetCanvasSize();
    }

    function SetCanvasSize() {
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        canvasWidth = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsWide`).val()) * uiScale;
        canvasHeight = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsHigh`).val()) * uiScale;

        ledPanelCanvas[panelMatrixID].setWidth(canvasWidth);
        ledPanelCanvas[panelMatrixID].setHeight(canvasHeight);
        ledPanelCanvas[panelMatrixID].calcOffset();
        ledPanelCanvas[panelMatrixID].renderAll();
    }

    function InitializeCanvas(panelMatrixID, reinit) {
        canvasInitialized = 1;

        if (reinit) {
            selectedPanel = -1;
            for (var key in panelGroups[panelMatrixID]) {
                ledPanelCanvas[panelMatrixID].remove(panelGroups[panelMatrixID][key].group);
            }
            panelGroups[panelMatrixID] = [];
        } else {
            ledPanelCanvas[panelMatrixID] = new fabric.Canvas(`ledPanelCanvas${panelMatrixID}`,
                {
                    backgroundColor: '#a0a0a0',
                    selection: false
                });
        }

        SetCanvasSize();

        if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].type === "LEDPanelMatrix") {
            for (var i = 0; i < channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels.length; i++) {
                SetupCanvasPanel(panelMatrixID, i);
            }
        }

        UpdateMatrixSize(panelMatrixID);

        ledPanelCanvas[panelMatrixID].on('object:moving', panelMovingHandler);
        ledPanelCanvas[panelMatrixID].on('selection:created', panelSelectedHandler);
        ledPanelCanvas[panelMatrixID].on('selection:updated', panelSelectedHandler);
        ledPanelCanvas[panelMatrixID].on('selection:cleared', panelSelectedHandler);
    }

    function ToggleAdvancedLayout() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();

        if ($(`#panelMatrix${panelMatrixID}  .LEDPanelUIAdvancedLayout`).is(":checked")) {
            if (typeof channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID] === 'undefined') {
                SaveChannelOutputsJSON();
            }

            $(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsWide`).val(LEDPanelWidth * (LEDPanelCols[panelMatrixID] + 1));
            $(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsHigh`).val(LEDPanelHeight * (LEDPanelRows[panelMatrixID] + 1));

            if (canvasInitialized)
                InitializeCanvas(panelMatrixID, 1);
            else
                InitializeCanvas(panelMatrixID, 0);

            UpdateMatrixSize(panelMatrixID);
            $(`#panelMatrix${panelMatrixID} .ledPanelSimpleUI`).hide();
            $(`#panelMatrix${panelMatrixID} .ledPanelCanvasUI`).show();
        } else {
            UpdateLegacyLEDPanelLayout(panelMatrixID);
            $(`#panelMatrix${panelMatrixID} .ledPanelCanvasUI`).hide();
            $(`#panelMatrix${panelMatrixID} .ledPanelSimpleUI`).show();
        }
    }

    function cpOutputNumberChanged() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        if (selectedPanel < 0)
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[selectedPanel].outputNumber = parseInt($(`#panelMatrix${panelMatrixID} .cpOutputNumber`).val());
        SetupCanvasPanel(panelMatrixID, selectedPanel);
    }

    function cpPanelNumberChanged() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        if (selectedPanel < 0)
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[selectedPanel].panelNumber = parseInt($(`#panelMatrix${panelMatrixID} .cpPanelNumber`).val());
        SetupCanvasPanel(panelMatrixID, selectedPanel);
    }

    function cpColorOrderChanged() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();
        if (selectedPanel < 0)
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[selectedPanel].colorOrder = $(`#panelMatrix${panelMatrixID} .cpColorOrder`).val();
        SetupCanvasPanel(panelMatrixID, selectedPanel);
    }

    function SetupAdvancedUISelects(panelMatrixID) {


        for (var i = 0; i < LEDPanelOutputs; i++) {
            $(`#panelMatrix${panelMatrixID} .cpOutputNumber`).append("<option value='" + i + "'>" + (i + 1) + "</option>");
        }

        for (var i = 0; i < LEDPanelPanelsPerOutput; i++) {
            $(`#panelMatrix${panelMatrixID} .cpPanelNumber`).append("<option value='" + i + "'>" + (i + 1) + "</option>");
        }
    }

    function SetSinglePanelSize(panelMatrixID) {
        if (!("LEDPanelMatrix" in channelOutputsLookup))
            return;

        var w = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelWidth;
        var h = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelHeight;
        var s = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelScan;
        var singlePanelSize = [w, h, s].join('x');
        if ("panelAddressing" in channelOutputsLookup) {
            singlePanelSize = [singlePanelSize, channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelAddressing].join('x');
        }
        $(`#panelMatrix${panelMatrixID} .LEDPanelsSize`).val(singlePanelSize.toString());
    }

    function GetCurrentActiveMatrixPanelID() {
        return $("[id^=panelMatrix].tab-pane.active")[0].firstElementChild.innerText;
    }

    function PanelSubtypeChanged(panelMatrixID) {
        var select = document.getElementById("panelMatrix" + panelMatrixID).getElementsByClassName("printSettingFieldCola col-md-3 col-lg-3");
        var html = "";

        html += "<select class='LEDPanelsSize' onchange='LEDPanelsSizeChanged(GetCurrentActiveMatrixPanelID());'>"

        <? if ($settings['BeaglePlatform']) { ?>
            html += "<option value='32x16x8'>32x16 1/8 Scan</option>"
            html += "<option value='32x16x4'>32x16 1/4 Scan</option>"
            html += "<option value='32x16x4x1'>32x16 1/4 Scan ABCD</option>"
            html += "<option value='32x16x2'>32x16 1/2 Scan A</option>"
            html += "<option value='32x16x2x1'>32x16 1/2 Scan AB</option>"
            html += "<option value='32x32x16'>32x32 1/16 Scan</option>"
            html += "<option value='64x32x16'>64x32 1/16 Scan</option>"
            <? if ($panelCapesHaveSel4) { ?>
                html += "<option value='64x64x32'>64x64 1/32 Scan</option>"
            <? } ?>
            html += "<option value='64x32x8'>64x32 1/8 Scan</option>"
            html += "<option value='32x32x8'>32x32 1/8 Scan</option>"
            html += "<option value='40x20x5'>40x20 1/5 Scan</option>"
            html += "<option value='80x40x10'>80x40 1/10 Scan</option>"
            <? if ($panelCapesHaveSel4) { ?>
                html += "<option value='80x40x20'>80x40 1/20 Scan</option>"
            <? } ?>
        <? } else { ?>
            html += "<option value='32x16x8'>32x16 1/8 Scan</option>"
            html += "<option value='32x16x4'>32x16 1/4 Scan</option>"
            html += "<option value='32x16x2'>32x16 1/2 Scan</option>"
            html += "<option value='32x32x16'>32x32 1/16 Scan</option>"
            html += "<option value='32x32x8'>32x32 1/8 Scan</option>"
            html += "<option value='64x32x16'>64x32 1/16 Scan</option>"
            html += "<option value='64x32x8'>64x32 1/8 Scan</option>"
            html += "<option value='64x64x8'>64x64 1/8 Scan</option>"
            if ($(`#panelMatrix${panelMatrixID}` + " .LEDPanelsConnectionType").text() === 'ColorLight5a75') {
                html += "<option value='48x48x6'>48x48 1/6 Scan</option>"
                html += "<option value='80x40x10'>80x40 1/10 Scan</option>"
                html += "<option value='80x40x20'>80x40 1/20 Scan</option>"
            }
            html += "<option value='128x64x32'>128x64 1/32 Scan</option>"
        <? } ?>

        html += "</select>"

        select.item(0).innerHTML = html

        SetSinglePanelSize(panelMatrixID);
    }

    function AutoLayoutPanels() {
        //get currently visible panelMatrixID
        panelMatrixID = $('.panelMatrix-tab-content .tab-pane.active .divPanelMatrixID')[0].innerHTML;
        var sure = prompt('WARNING: This will re-configure all output and panel numbers with one output per row and all panels facing in an upwards direction.  Are you sure? [Y/N]', 'N');

        if (sure.toUpperCase() != 'Y')
            return;

        var panelsWide = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutCols`).val());
        var panelsHigh = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutRows`).val());

        for (var y = 0; y < panelsHigh; y++) {
            for (var x = 0; x < panelsWide; x++) {
                var panel = panelsWide - x - 1;
                $(`#panelMatrix${panelMatrixID} .LEDPanelPanelNumber_${y}_${panel}`).val(x);
                $(`#panelMatrix${panelMatrixID} .LEDPanelOutputNumber_${y}_${panel}`).val(y);
                $(`#panelMatrix${panelMatrixID} .LEDPanelOrientation_${y}_${panel}`).attr('src', 'images/arrow_N.png');
                $(`#panelMatrix${panelMatrixID} .LEDPanelColorOrder_${y}_${panel}`).val('');
            }
        }
        SaveChannelOutputsJSON();
    }


    function TogglePanelTestPattern() {
        var val = $("#PanelTestPatternButton").val();
        if (val == "Test Pattern") {
            var outputType = $('#LEDPanelsConnection').val();
            if (outputType == "ColorLight5a75") {
                outputType = "ColorLight Panels";
            } else if (outputType == "RGBMatrix") {
                outputType = "Pi Panels";
            } else if (outputType == "BBBMatrix" || outputType == "LEDscapeMatrix") {
                outputType = "BBB Panels";
            }
            $("#PanelTestPatternButton").val("Stop Pattern");
            var data = '{"command":"Test Start","multisyncCommand":false,"multisyncHosts":"","args":["1000","Output Specific","' + outputType + '","1"]}';
            $.post("api/command", data
            ).done(function (data) {
            }).fail(function () {
            });
        } else {
            $("#PanelTestPatternButton").val("Test Pattern");
            var data = '{"command":"Test Stop","multisyncCommand":false,"multisyncHosts":"","args":[]}';
            $.post("api/command", data
            ).done(function (data) {
            }).fail(function () {
            });
        }
    }

    function AddPanelMatrixDialog() {
        var options = {
            id: 'AddPanelMatrixDialog',
            title: 'Add a new LED Panel Matrix Output',
            class: 'modal-sm',
            noClose: false,
            backdrop: 'static',
            keyboard: true,
            buttons: {
                'Add Panel': {
                    disabled: false,
                    id: 'RebootButton',
                    class: 'btn-danger',
                    click: function () {
                        CloseModalDialog('AddPanelMatrixDialog');
                        location.reload();
                    }
                },
                Abort: {
                    disabled: false,
                    id: 'AbortButton',
                    click: function () {
                        CloseModalDialog('AddPanelMatrixDialog');
                        location.reload();
                    }
                }
            }
        };

        options.body = document.querySelector("#AddPanelDialogCode").innerHTML;
        DoModalDialog(options);

    }

    function WarnIfSlowNIC(panelMatrixID = GetCurrentActiveMatrixPanelID()) {
        var NicSpeed = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).find(":selected").text().split('(')[1].split('M')[0]);
        if (NicSpeed < 1000 && $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`).find(":selected").text() == "ColorLight" && $(`#panelMatrix${panelMatrixID} .LEDPanelsEnabled`).is(":checked") == true) {
            $(`#panelMatrix${panelMatrixID} .divLEDPanelWarnings`).html(`<div class="alert alert-danger">Selected interface for Panel Matrix ${panelMatrixID} does not support 1000+ Mbps, which is the Colorlight minimum</div>`);
        } else {
            $(`#panelMatrix${panelMatrixID} .divLEDPanelWarnings`).html("");
        }
    }

    function LEDPanelsSizeChanged(panelMatrixID) {
        var value = $(`#panelMatrix${panelMatrixID} .LEDPanelsSize`).val();
        SetSetting("LEDPanelsSize", value, 1, 0, false, null, function () {
            settings['LEDPanelsSize'] = value;
            LEDPanelLayoutChanged();
        });
    }

    function populatePanelMatrixTab(panelMatrixID) {
        //copy from template
        document.querySelector(`#panelMatrix${panelMatrixID}`).innerHTML = document.querySelector('#divLEDPanelsTemplate').innerHTML;
        //Set the PanelMatrixID
        document.querySelector(`#panelMatrix${panelMatrixID}  .divPanelMatrixID`).innerHTML = panelMatrixID;
        //Rename the Canvas id
        $(`#panelMatrix${panelMatrixID} canvas.ledPanelCanvas`).attr("id", "ledPanelCanvas" + panelMatrixID)
    }

    $(document).ready(function () {

        populatePanelMatrixTab(1);
        // populatePanelMatrixTab(2);
        //  populatePanelMatrixTab(3);


        InitializeLEDPanelMatrix(1);
        //  InitializeLEDPanelMatrix(2);
        LEDPanelsConnectionChanged(1);

        SetupAdvancedUISelects(1);
        panelMatrixID = GetCurrentActiveMatrixPanelID();

        if ($(`#panelMatrix${panelMatrixID} .LEDPanelUIAdvancedLayout`).is(":checked")) {
            InitializeCanvas(panelMatrixID, 0);
            //InitializeCanvas(1, 0);
            $(`#panelMatrix${panelMatrixID} .ledPanelSimpleUI`).hide();
            $(`#panelMatrix${panelMatrixID} .ledPanelCanvasUI`).show();
        } else {
            $(`#panelMatrix${panelMatrixID} .ledPanelCanvasUI`).hide();
        }

        <?
        if (
            (isset($settings['cape-info'])) &&
            ((in_array('all', $settings['cape-info']["provides"])) ||
                (in_array('panels', $settings['cape-info']["provides"])))
        ) {
            ?>
            if (currentCapeName != "" && currentCapeName != "Unknown") {
                $('.capeNamePanels').html(currentCapeName);
                $('.capeTypeLabel').html("Cape Config");
            }

            <?
        }
        ?>
        WarnIfSlowNIC(1);
    });

</script>
<div id="divLEDPanelMatrices">
    <div class="row tableTabPageHeader">
        <div class="col-md">
            <h2><span class='capeNamePanels'>LED Panel Matrices</span> </h2>
        </div>
        <div class="col-md-auto ms-lg-auto">
            <div class="form-actions">
                <input id="AddPanelMatrixButton" type='button' class="buttons ms-1" onClick='AddPanelMatrixDialog();'
                    value='Add Panel Matrix'>
                <input id="PanelTestPatternButton" type='button' class="buttons ms-1"
                    onClick='TogglePanelTestPattern();' value='Test Pattern'>
                <input type='button' class="buttons btn-success ms-1" onClick='SaveChannelOutputsJSON();' value='Save'>
            </div>
        </div>
    </div>
    <!-- LED Panel Matrix Tabs --->
    <ul class="nav nav-tabs" id="panelTabs" role="tablist">
        <li class="nav-item"><a class="nav-link" href="#panelMatrix1" data-bs-toggle="tab"
                data-bs-target="#panelMatrix1">Panel Matrix 1</a></li>
        <li class="nav-item"><a class="nav-link" href="#panelMatrix2" data-bs-toggle="tab"
                data-bs-target="#panelMatrix2">Panel Matrix 2</a></li>
        <li class="nav-item"><a class="nav-link" href="#panelMatrix3" data-bs-toggle="tab"
                data-bs-target="#panelMatrix3">Panel Matrix 3</a></li>
        <li class="nav-item"><a class="nav-link" href="#panelMatrix4" data-bs-toggle="tab"
                data-bs-target="#panelMatrix4">Panel Matrix 4</a></li>
        <li class="nav-item"><a class="nav-link" href="#panelMatrix5" data-bs-toggle="tab"
                data-bs-target="#panelMatrix5">Panel Matrix 5</a></li>
    </ul>

    <!-- LED Panel Matrix Tab-Content --->

    <div class="panelMatrix-tab-content tab-content" style="border:1px;border-style: solid;">
        <div class="tab-pane active" id="panelMatrix1">panel 1 content</div>
        <div class="tab-pane" id="panelMatrix2">panel 2 content</div>
        <div class="tab-pane" id="panelMatrix3">panel 3 content</div>
        <div class="tab-pane" id="panelMatrix4">panel 4 content</div>
        <div class="tab-pane" id="panelMatrix5">panel 5 content</div>
    </div>

    <div id="divNotes">
        <br>
        <b>Notes and hints:</b>
        <ul>
            <? if (count($panelCapes) == 1) {
                if (isset($panelCapes[0]["warnings"][$settings["SubPlatform"]])) {
                    echo "<li><font color='red'>" . $panelCapes[0]["warnings"][$settings["SubPlatform"]] . "</font></li>\n";
                }
                if (isset($panelCapes[0]["warnings"]["all"])) {
                    echo "<li><font color='red'>" . $panelCapes[0]["warnings"]["all"] . "</font></li>\n";
                }
                if (isset($panelCapes[0]["warnings"]["*"])) {
                    echo "<li><font color='red'>" . $panelCapes[0]["warnings"]["*"] . "</font></li>\n";
                }
            } ?>
            <li>
                <font color='red'>New Colorlight receiver firmware eg 13.x is currently incompatible with FPP, please
                    see <a href="https://github.com/FalconChristmas/fpp/issues/1849">Issue 1849</a></font>
            </li>
            <li>When wiring panels, divide the panels across as many outputs as possible. Shorter chains on more outputs
                will have higher refresh than longer chains on fewer outputs.</li>
            <li>If not using all outputs, use all the outputs from 1 up to what is needed. Data is always sent on
                outputs up to the highest configured, even if no panels are attached.</li>
            <? if ($settings['Platform'] == "Raspberry Pi") { ?>
            <li>The FPP developers strongly encourage using either a BeagleBone based panel driver (Octoscroller,
                PocketScroller) or using a ColorLight controller. The Raspberry Pi panel code performs poorly compared
                to the other options and supports a much more limited set of options.</li>
            <? } ?>
        </ul>
    </div> <!--close notes div -->


    <!-- TEMPLATE HTML USED AS CODE BASE ON EACH PANEL TAB -->


    <div id='divLEDPanelsTemplate' style="background-color: aquamarine;display:none;">
        <div class="divPanelMatrixID" style="display:none;"></div>

        <div class="divLEDPanelWarnings">
        </div>

        <div class="tableOptionsForm">
            <div class="backdrop row">
                <div class="col-md-auto">
                    <div class="backdrop-dark form-inline enableCheckboxWrapper">
                        <div><b>Enable <span class='capeNamePanels'>Led Panel Matrix</span>:&nbsp;</b></div>
                        <div><input class='LEDPanelsEnabled' type='checkbox' onChange="WarnIfSlowNIC();"></div>
                    </div>
                </div>
                <div class="col-md-auto form-inline">
                    <div class='LEDPanelsConnectionLabel'><b>Connection Type:&nbsp;</b>
                        <div>
                            <select class='LEDPanelsConnectionSelect' onChange='LEDPanelsConnectionChanged();'>
                                <?
                                if (
                                    in_array('all', $currentCapeInfo["provides"])
                                    || in_array('panels', $currentCapeInfo["provides"])
                                ) {
                                    if ($settings['Platform'] == "Raspberry Pi") {
                                        ?>
                                        <option value='RGBMatrix'>Hat/Cap/Cape</option>
                                        <?
                                    } else if ($settings['Platform'] == "BeagleBone Black") { ?>
                                            <option value='LEDscapeMatrix'>Hat/Cap/Cape</option>
                                    <? } ?>
                                    <?
                                } ?>
                                <option value='ColorLight5a75'>ColorLight</option>
                                <?
                                if ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux")) {
                                    echo "<option value='X11PanelMatrix'>X11 Panel Matrix</option>\n";
                                }
                                ?>
                            </select>
                        </div>
                    </div>
                    <div class="LEDPanelsConnectionInterface col-md-auto form-inline">
                        <b>Interface:</b>
                        <select class='LEDPanelsInterface' type='hidden' onChange='WarnIfSlowNIC();'>
                            <? PopulateEthernetInterfaces(); ?>
                        </select>
                    </div>
                    <div class="LEDPanelsWiringPinoutLabel col-md-auto form-inline">
                        <b>Wiring Pinout:</b>
                        <select class='LEDPanelsWiringPinout'>
                            <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                                <option value='regular'>Standard</option>
                                <option value='adafruit-hat'>Adafruit</option>
                                <option value='adafruit-hat-pwm'>Adafruit PWM</option>
                                <option value='regular-pi1'>Standard Pi1</option>
                                <option value='classic'>Classic</option>
                                <option value='classic-pi1'>Classic Pi1</option>
                            <? } else if ($settings['BeaglePlatform']) {
                                if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) { ?>
                                        <option value='v2'>v2.x</option>
                                <? } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) { ?>
                                            <option value='PocketScroller1x'>PocketScroller</option>
                                <? } else { ?>
                                            <option value='v1'>Standard v1.x</option>
                                            <option value='v2'>v2.x</option>
                                <? } ?>
                                <?
                            } ?>
                        </select>
                    </div>

                    <div class="col-md-auto ms-lg-auto">
                        <div class="form-actions">
                            <input id="RemovePanelMatrixButton" type='button' class="buttons ms-1 btn-danger"
                                onClick='RemovePanelMatrixConfig();' value='Remove Panel Matrix'>
                        </div>
                    </div>


                </div>
            </div>
            <div class='divLEDPanelsData'>
                <div class="container-fluid settingsTable">
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2">
                            <span class='ledPanelSimpleUI'><b>Panel Layout:</b></span>
                            <span class='ledPanelCanvasUI'><b>Matrix Size (WxH):</b></span>
                        </div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <span class='ledPanelSimpleUI LEDPanelLayoutSelect'>
                                <!--  W:<select class='LEDPanelsLayoutCols' onChange='LEDPanelsLayoutChanged()'></select>
                                H:<select class='LEDPanelsLayoutRows' onChange='LEDPanelsLayoutChanged()'></select> -->


                                <? printLEDPanelLayoutSelect(1); ?>
                                <input type='button' class='buttons' onClick='AutoLayoutPanels();' value='Auto Layout'>
                            </span>
                            <span class='ledPanelCanvasUI'>
                                <span class='matrixSize'></span>
                            </span>
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Start Channel:</b></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <input class='LEDPanelsStartChannel' type=number min=1 max=<?= FPPD_MAX_CHANNELS ?>
                                value='1'>
                        </div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Single Panel Size (WxH):</b></div>
                        <div class="printSettingFieldCola col-md-3 col-lg-3">
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Channel Count:</b></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3"><span
                                class='LEDPanelsChannelCount'>1536</span>(<span class='LEDPanelsPixelCount'>512</span>
                            Pixels)</div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Model Start Corner:</b></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <select class='LEDPanelsStartCorner'>
                                <option value='0'>Top Left</option>
                                <option value='1'>Bottom Left</option>
                            </select>
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Default Panel Color Order:</b></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <select class='LEDPanelsColorOrder'>
                                <option value='RGB'>RGB</option>
                                <option value='RBG'>RBG</option>
                                <option value='GRB'>GRB</option>
                                <option value='GBR'>GBR</option>
                                <option value='BRG'>BRG</option>
                                <option value='BGR'>BGR</option>
                            </select>
                        </div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Panel Gamma:</b></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <? printLEDPanelGammaSelect($settings['Platform'], $LEDPanelGamma); ?>
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                class='LEDPanelsBrightnessLabel'><b>Brightness:</b></span></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <select class='LEDPanelsBrightness'>
                                <?
                                if ($settings['Platform'] == "Raspberry Pi") {
                                    for ($x = 100; $x >= 10; $x -= 5) {
                                        echo "<option value='$x'>$x%</option>\n";
                                    }

                                } else {
                                    for ($x = 10; $x >= 1; $x -= 1) {
                                        echo "<option value='$x'>$x</option>\n";
                                    }

                                }
                                ?>
                            </select>
                        </div>
                        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                            <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                    class='LEDPanelsGPIOSlowdownLabel'><b>GPIO Slowdown:</b></span></div>
                            <div class="printSettingFieldCol col-md-3 col-lg-3">
                                <select class='LEDPanelsGPIOSlowdown'>
                                    <option value='0'>0 (Pi Zero and other single-core)</option>
                                    <option value='1' selected>1 (multi-core Pi)</option>
                                    <option value='2'>2 (slow panels)</option>
                                    <option value='3'>3 (slower panels)</option>
                                    <option value='4'>4 (slower panels or faster Pi)</option>
                                    <option value='5'>5 (slower panels or faster Pi)</option>
                                </select>
                            </div>

                        <? } else if ($settings['BeaglePlatform']) { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                        class='LEDPanelsOutputByRowLabel'><b>Output By Row:</b></div>
                                <div class="printSettingFieldCol col-md-3 col-lg-3"><input class='LEDPanelsOutputByRow'
                                        type='checkbox' onclick='outputByRowClicked()'></div>
                        <? } else { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                                <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                        <? } ?>

                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                class='LEDPanelsInterleaveLabel'><b>Panel Interleave:</b></span></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <? printLEDPanelInterleaveSelect(); ?>
                        </div>
                        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                            <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                    class='LEDPanelsOutputCPUPWMLabel'><b>Use CPU PWM:</b></span></div>
                            <div class="printSettingFieldCol col-md-3 col-lg-3"><input class='LEDPanelsOutputCPUPWM'
                                    type='checkbox'></div>
                        <? } else if ($settings['BeaglePlatform']) { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                        class='LEDPanelsOutputBlankRowLabel'><b>Blank between rows:</b></span></div>
                                <div class="printSettingFieldCol col-md-3 col-lg-3"><input class='LEDPanelsOutputBlankRow'
                                        type='checkbox'></div>
                        <? } else { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                                <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                        <? } ?>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                class='LEDPanelsColorDepthLabel'><b>Color Depth:</b></span></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3">
                            <select class='LEDPanelsColorDepth'>
                                <? if ($settings['BeaglePlatform']) { ?>
                                    <option value='12'>12 Bit</option>
                                <? } ?>
                                <option value='11'>11 Bit</option>
                                <option value='10'>10 Bit</option>
                                <option value='9'>9 Bit</option>
                                <option value='8' selected>8 Bit</option>
                                <option value='7'>7 Bit</option>
                                <option value='6'>6 Bit</option>
                            </select>
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                        <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                    </div>
                </div>
            </div>
            <div class="row">
                <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"><span class='LEDPanelsRowAddressTypeLabel'><b>Panel
                                Row Address Type:</b></span></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3">
                        <select class='LEDPanelRowAddressType'>
                            <option value='0' selected>Standard</option>
                            <option value='1'>AB-Addressed Panels</option>
                            <option value='2'>Direct Row Select</option>
                            <option value='3'>ABC-Addressed Panels</option>
                            <option value='4'>ABC Shift + DE Direct</option>
                        </select>
                    </div>
                <? } else { ?>
                    <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                    <div class="printSettingFieldCol col-md-3 col-lg-3"></div>
                <? } ?>
            </div>


            <div class='divLEDPanelsLayoutData'>
                <div style="padding: 10px;">
                    <br>
                    <b>LED Panel Layout:</b><br>
                    Advanced Layout?
                    <? PrintSettingCheckbox("Advanced Layout", "LEDPanelUIAdvancedLayout", 0, 0, "1", "0", "", "ToggleAdvancedLayout", 0); ?>
                    <script>$('#LEDPanelUIAdvancedLayout').addClass('LEDPanelUIAdvancedLayout').removeAttr("id");</script>
                    <br>
                    <span class='ledPanelSimpleUI'>
                        View Config from front?
                        <? PrintSettingCheckbox("Front View", "LEDPanelUIFrontView", 0, 0, "1", "0", "", "FrontBackViewToggled", 1); ?>
                        <script>$('#LEDPanelUIFrontView').addClass('LEDPanelUIFrontView').removeAttr("id");</script>
                    </span>
                    <span class='ledPanelCanvasUI'>Front View</span>
                    <br>
                    <div style='overflow: auto;'>
                        <table class='ledPanelCanvasUI' style='display:none;'>
                            <tr>
                                <td><canvas id='ledPanelCanvas' class='ledPanelCanvas' width='900' height='400'
                                        style='border: 2px solid rgb(0,0,0);'></canvas></td>
                                <td></td>
                                <td>
                                    <b>Selected Panel:</b><br>
                                    <table>
                                        <tr>
                                            <td>Output:</td>
                                            <td>
                                                <select class='cpOutputNumber' onChange='cpOutputNumberChanged();'>
                                                </select>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>Panel:</td>
                                            <td>
                                                <select class='cpPanelNumber' onChange='cpPanelNumberChanged();'>
                                                </select>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>Color:</td>
                                            <td>
                                                <select class='cpColorOrder' onChange='cpColorOrderChanged();'>
                                                    <option value=''>Def</option>
                                                    <option value='RGB'>RGB</option>
                                                    <option value='RBG'>RBG</option>
                                                    <option value='GBR'>GBR</option>
                                                    <option value='GRB'>GRB</option>
                                                    <option value='BGR'>BGR</option>
                                                    <option value='BRG'>BRG</option>
                                                </select>
                                            </td>
                                        </tr>
                                        <tr>
                                            <td>X Pos:</td>
                                            <td><span class='cpXOffset'></span></td>
                                        </tr>
                                        <tr>
                                            <td>Y Pos:</td>
                                            <td><span class='cpYOffset'></span></td>
                                        </tr>
                                        <tr>
                                            <td colspan=2><input type='button' onclick='RotateCanvasPanel();'
                                                    value='Rotate'></td>
                                        </tr>
                                        <!--
                                    <tr><td>Snap:</td><td><input type='checkbox' id='cpSnap'></td></tr>
                                    -->
                                    </table>
                                </td>
                            </tr>
                            <tr>
                                <td colspan=3>
                                    <table>
                                        <tr>
                                            <td><b>UI Layout Size:</b></td>
                                            <td>Pixels Wide:</td>
                                            <td><? PrintSettingTextSaved("LEDPanelUIPixelsWide", 2, 0, 4, 4, "", "256", "SetCanvasSize", "", "text", [], "class"); ?>
                                            </td>
                                            <td></td>
                                            <td>Pixels High:</td>
                                            <td><? PrintSettingTextSaved("LEDPanelUIPixelsHigh", 2, 0, 4, 4, "", "128", "SetCanvasSize", "", "text", [], "class"); ?>
                                            </td>
                                        </tr>
                                    </table>
                                </td>
                            </tr>
                        </table>
                        <div class='ledPanelSimpleUI'>
                            <table class='LEDPanelTable'>
                                <tbody>
                                </tbody>
                            </table>
                        </div>
                        - O-# is physical output number.<br>
                        - P-# is panel number on physical output.<br>
                        - C-(color) is color order if panel has different color order than default (C-Def).<br>
                        - Arrow <img src='images/arrow_N.png' height=17 width=17 alt="panel orientation"> indicates
                        panel orientation, click arrow to rotate.<br>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <!-- END OF PANEL MATRIX TEMPLATE  -->




    <div id="AddPanelDialogCode" style="display:none;">
        <div id='LEDPanelsConnectionSelectLabel'>
            <b>Select Connection Type:&nbsp;</b>
        </div>
        <div>
            <select id='LEDPanelsConnectionSelect' onChange='LEDPanelsConnectionChanged();'>
                <?
                if (
                    in_array('all', $currentCapeInfo["provides"])
                    || in_array('panels', $currentCapeInfo["provides"])
                ) {
                    if ($settings['Platform'] == "Raspberry Pi") {
                        ?>
                        <option value='RGBMatrix'>Hat/Cap/Cape</option>
                        <?
                    } else if ($settings['Platform'] == "BeagleBone Black") { ?>
                            <option value='LEDscapeMatrix'>Hat/Cap/Cape</option>
                    <? } ?>
                    <?
                } ?>
                <option value='ColorLight5a75'>ColorLight</option>
                <?
                if ((file_exists('/usr/include/X11/Xlib.h')) && ($settings['Platform'] == "Linux")) {
                    echo "<option value='X11PanelMatrix'>X11 Panel Matrix</option>\n";
                }
                ?>
            </select>
        </div>
    </div>
</div>