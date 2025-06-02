<script src='js/fabric/fabric.min.js'></script>
<script>

    <?

    //Get Cape Information
    $panelCapes = array();
    $panelCapes = readPanelCapes($mediaDirectory . "/tmp/panels/", $panelCapes);
    $panelCapesHaveSel4 = false;
    $panelCapesDriver = "";
    $panelCapesName = "";

    //set default values
    $LEDPanelDefaults = [];
    $LEDPanelDefaults["LEDPanelsEnabled"] = 1;
    $LEDPanelDefaults["LEDPanelOutputs"] = 24;
    $LEDPanelDefaults["LEDPanelsPanelsPerOutput"] = 24;
    $LEDPanelDefaults["LEDPanelWidth"] = 32;
    $LEDPanelDefaults["LEDPanelHeight"] = 16;
    $LEDPanelDefaults["LEDPanelScan"] = 8;
    $LEDPanelDefaults["LEDPanelAddressing"] = 0;
    $LEDPanelDefaults["LEDPanelGamma"] = 2.2;
    $LEDPanelDefaults["LEDPanelColorOrder"] = "RGB";
    $LEDPanelDefaults["ledPanelsLayout"] = "2x2";
    $LEDPanelDefaults["LEDPanelCols"] = 2;
    $LEDPanelDefaults["LEDPanelRows"] = 2;
    $LEDPanelDefaults["LEDPanelsWiringPinout"] = "regular";
    $LEDPanelDefaults["gpioSlowdown"] = 0;
    $LEDPanelDefaults["brightness"] = 100;
    $LEDPanelDefaults["LEDPanelsOutputCPUPWM"] = 0;
    $LEDPanelDefaults['LEDPanelType'] = 0;
    $LEDPanelDefaults['invertedData'] = 0; // Default to no inversion - Top Left
    $LEDPanelDefaults['LEDPanelInterleave'] = 0; // Default to no interleaving
    $LEDPanelDefaults['LEDPanelUIAdvancedLayout'] = 0; // Default to standard layout
    $LEDPanelDefaults['LEDPanelUIFrontView'] = 0; // Default to back view
    $LEDPanelDefaults['LEDPanelUIPixelsHigh'] = 192; // Default to 192 pixels high
    $LEDPanelDefaults['LEDPanelUIPixelsWide'] = 128; // Default to 128 pixels wide
    $LEDPanelDefaults['LEDPanelsSize'] = "32x16x8"; // Default to 32x16x8
    
    if (count($panelCapes) == 1) {
        echo "var KNOWN_PANEL_CAPE = " . $panelCapes[0] . ";";
        $panelCapes[0] = json_decode($panelCapes[0], true);
        if (isset($panelCapes[0]["controls"]["sel4"])) {
            $panelCapesHaveSel4 = true;
        }
        if (isset($panelCapes[0]["driver"])) {
            $panelCapesDriver = $panelCapes[0]["driver"];
        }
        if (isset($panelCapes[0]["name"])) {
            $panelCapesName = $panelCapes[0]["name"];
        }
    } else {
        echo "// NO KNOWN_PANEL_CAPE";
    }

    if ($settings['BeaglePlatform']) {
        $LEDPanelDefaults["LEDPanelsPanelsPerOutput"] = 16;
        if (count($panelCapes) == 1) {
            $LEDPanelDefaults["LEDPanelOutputs"] = count($panelCapes[0]["outputs"]);
        } else if (strpos($settings['SubPlatform'], 'Green Wireless') !== false) {
            $LEDPanelDefaults["LEDPanelOutputs"] = 5;
        } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
            $LEDPanelDefaults["LEDPanelOutputs"] = 6;
        } else {
            $LEDPanelDefaults["LEDPanelOutputs"] = 8;
        }
    }

    if ($settings['Platform'] == "Raspberry Pi") {
        $LEDPanelDefaults["brightness"] = 100;
    } else {
        $LEDPanelDefaults["brightness"] = 10;
    }

    $LEDPanelDefaults["maxLEDPanels"] = $LEDPanelDefaults["LEDPanelOutputs"] * $LEDPanelDefaults["LEDPanelsPanelsPerOutput"];
    $LEDPanelDefaults["maxLEDPanels"] = 96; // Override to allow different panel configs using ColorLight cards
    // End of Default Value Logic
    
    //Setup PHP MatricesArray
    $matricesArray = [];
    if (isset($channelOutputs["channelOutputs"]) && !empty($channelOutputs["channelOutputs"])) {
        foreach ($channelOutputs["channelOutputs"] as $output) {
            if ($output["type"] == "LEDPanelMatrix") {
                $matricesArray[] = $output;
            }
        }

        for ($z = 0; $z < count($matricesArray); $z++) {
            //Temporary Code to help handle migrating from old LEDPanelMatrix to new LEDPanelMatrix v2 ->v3 
    
            if (!isset($matricesArray[$z]["panelMatrixID"])) {
                $matricesArray[$z]["panelMatrixID"] = $z + 1;
            }

            if (!isset($matricesArray[$z]["ledPanelsLayout"]) && isset($settings["LEDPanelsLayout"])) {
                $matricesArray[$z]["ledPanelsLayout"] = $settings["LEDPanelsLayout"];
            }

            if (!isset($matricesArray[$z]["LEDPanelsSize"]) && isset($settings["LEDPanelsSize"])) {
                $matricesArray[$z]["LEDPanelsSize"] = $settings["LEDPanelsSize"];
            }

            if (!isset($matricesArray[$z]["advanced"]) && isset($settings["LEDPanelUIAdvancedLayout"])) {
                $matricesArray[$z]["advanced"] = $settings["LEDPanelUIAdvancedLayout"];
            }

            if (!isset($matricesArray[$z]["LEDPanelUIFrontView"]) && isset($settings["LEDPanelUIFrontView"])) {
                $matricesArray[$z]["LEDPanelUIFrontView"] = $settings["LEDPanelUIFrontView"];
            }

            if (!isset($matricesArray[$z]["LEDPanelUIPixelsHigh"]) && isset($settings["LEDPanelUIPixelsHigh"])) {
                $matricesArray[$z]["LEDPanelUIPixelsHigh"] = $settings["LEDPanelUIPixelsHigh"];
            }
            if (!isset($matricesArray[$z]["LEDPanelUIPixelsWide"]) && isset($settings["LEDPanelUIPixelsWide"])) {
                $matricesArray[$z]["LEDPanelUIPixelsWide"] = $settings["LEDPanelUIPixelsWide"];
            }

            if (!isset($matricesArray[$z]["panelInterleave"]) && isset($settings["panelInterleave"])) {
                $matricesArray[$z]["interLeave"] = $settings["interLeave"];
            }

            if (!isset($matricesArray[$z]["gpioSlowdown"]) && isset($settings["gpioSlowdown"])) {
                $matricesArray[$z]["gpioSlowdown"] = $settings["gpioSlowdown"];
            }

            if (isset($matricesArray[$z]["brightness"]) && $settings['Platform'] == "Raspberry Pi") {
                $matricesArray[$z]["brightness"] = round($matricesArray[$z]["brightness"] / 5) * 5; //round to selectable value
            }

            ////////////////////////////////////
            $panelMatrixID = $matricesArray[$z]["panelMatrixID"];
            $parts = explode('x', $matricesArray[$z]["ledPanelsLayout"]);
            if (count($parts) == 2) {
                $matricesArray[$z]["LEDPanelCols"] = $parts[0];
                $matricesArray[$z]["LEDPanelRows"] = $parts[1];
            }
        }
    } else {
        //set some defaults for the first panelMatrix
        $matricesArray[0]["type"] = "LEDPanelMatrix";
        $matricesArray[0]["panelMatrixID"] = 1;
        $matricesArray[0]["LEDPanelCols"] = $LEDPanelDefaults["LEDPanelCols"];
        $matricesArray[0]["LEDPanelRows"] = $LEDPanelDefaults["LEDPanelRows"];
    }

    //Populate any missing value in matrix Array with defaults
    foreach ($matricesArray as &$matrix) { // Use reference with & to modify the array directly
        if (!isset($matrix["ledPanelsLayout"])) {
            $matrix["ledPanelsLayout"] = $LEDPanelDefaults["ledPanelsLayout"];
        }
        if (!isset($matrix["LEDPanelsSize"])) {
            $matrix["LEDPanelsSize"] = $LEDPanelDefaults["LEDPanelsSize"];
        }
        if (!isset($matrix["ledPanelsGamma"])) {
            $matrix["ledPanelsGamma"] = $LEDPanelDefaults["LEDPanelGamma"];
        }
        if (!isset($matrix["ledPanelsOutputs"])) {
            $matrix["ledPanelsOutputs"] = $LEDPanelDefaults["LEDPanelOutputs"];
        }
        if (!isset($matrix["ledPanelsPanelsPerOutput"])) {
            $matrix["ledPanelsPanelsPerOutput"] = $LEDPanelDefaults["LEDPanelsPanelsPerOutput"];
        }
        if (!isset($matrix["ledPanelsWidth"])) {
            $matrix["ledPanelsWidth"] = $LEDPanelDefaults["LEDPanelWidth"];
        }
        if (!isset($matrix["ledPanelsHeight"])) {
            $matrix["ledPanelsHeight"] = $LEDPanelDefaults["LEDPanelHeight"];
        }
        if (!isset($matrix["ledPanelsScan"])) {
            $matrix["ledPanelsScan"] = $LEDPanelDefaults["LEDPanelScan"];
        }
        if (!isset($matrix["ledPanelsAddressing"])) {
            $matrix["ledPanelsAddressing"] = $LEDPanelDefaults["LEDPanelAddressing"];
        }
        if (!isset($matrix["ledPanelsColorOrder"])) {
            $matrix["ledPanelsColorOrder"] = $LEDPanelDefaults["LEDPanelColorOrder"];
        }
        if (!isset($matrix["enabled"])) {
            $matrix["enabled"] = $LEDPanelDefaults["LEDPanelsEnabled"];
        }
        if (!isset($matrix["startChannel"])) {
            $matrix["startChannel"] = 1;
        }
        if (!isset($matrix["channelCount"])) {
            $matrix["channelCount"] = 1;
        }
        if (!isset($matrix["brightness"])) {
            $matrix["brightness"] = $LEDPanelDefaults["brightness"];
        }
        if (!isset($matrix["gpioSlowdown"])) {
            $matrix["gpioSlowdown"] = $LEDPanelDefaults["gpioSlowdown"];
        }
        if (!isset($matrix["maxLEDPanels"])) {
            $matrix["maxLEDPanels"] = $LEDPanelDefaults["maxLEDPanels"];
        }
        if (!isset($matrix["wiringPinout"])) {
            $matrix["wiringPinout"] = $LEDPanelDefaults["LEDPanelsWiringPinout"];
        }
        if (!isset($matrix["LEDPanelsOutputCPUPWM"])) {
            $matrix["LEDPanelsOutputCPUPWM"] = $LEDPanelDefaults["LEDPanelsOutputCPUPWM"];
        }
        if (!isset($matrix["interLeave"])) {
            $matrix["interLeave"] = $LEDPanelDefaults["LEDPanelInterleave"];
        }
        if (!isset($matrix["advanced"])) {
            $matrix["advanced"] = $LEDPanelDefaults["LEDPanelUIAdvancedLayout"];
        }
        if (!isset($matrix["LEDPanelUIFrontView"])) {
            $matrix["LEDPanelUIFrontView"] = $LEDPanelDefaults["LEDPanelUIFrontView"];
        }
    }
    // Unset reference to avoid side effects
    unset($matrix);

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

    function readMatrixArrayValue($panelMatrixID, $key)
    {
        global $matricesArray;

        //find element matching panelMatrixID
        $matrixOfInterest = array_filter($matricesArray, function ($matrix) use ($panelMatrixID) {
            return isset($matrix["panelMatrixID"]) && $matrix["panelMatrixID"] == $panelMatrixID;
        });


        if (isset($matrixOfInterest[0][$key])) {
            return $matrixOfInterest[0][$key];
        } else {
            return "";
        }
    }

    function printLEDPanelLayoutSelect($panelMatrixID)
    {
        global $matricesArray;

        echo "W: <select class='LEDPanelsLayoutCols' onChange='LEDPanelsLayoutChanged();'>\n";
        for ($r = 1; $r <= readMatrixArrayValue($panelMatrixID, "maxLEDPanels"); $r++) {
            if (readMatrixArrayValue($panelMatrixID, "LEDPanelCols") == $r) {
                echo "<option value='" . $r . "' selected>" . $r . "</option>\n";
            } else {
                echo "<option value='" . $r . "'>" . $r . "</option>\n";
            }
        }
        echo "</select>";

        echo "&nbsp; H:<select class='LEDPanelsLayoutRows' onChange='LEDPanelsLayoutChanged();'>\n";
        for ($r = 1; $r <= readMatrixArrayValue($panelMatrixID, "maxLEDPanels"); $r++) {
            if (readMatrixArrayValue($panelMatrixID, "LEDPanelRows") == $r) {
                echo "<option value='" . $r . "' selected>" . $r . "</option>\n";
            } else {
                echo "<option value='" . $r . "'>" . $r . "</option>\n";
            }
        }
        echo "</select>";
    }

    function printLEDPanelGammaSelect($platform)
    {
        global $LEDPanelDefaults;

        $gamma = $LEDPanelDefaults["LEDPanelGamma"];

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
    //END OF PHP ---BEGINNING OF JAVASCRIPT FUNCTIONS ////////////////////////////////////////////////////////////////////////////////////////

    var LEDPanelDefaults = <?php echo json_encode($LEDPanelDefaults); ?>;

    //Define mappings of selectors to channelOutputsLookup
    const selectors = [
        ["LEDPanelsStartChannel", "startChannel"],
        ["LEDPanelsPixelCount", "channelCount", val => val / 3],
        ["LEDPanelsChannelCount", "channelCount"],
        ["LEDPanelsColorOrder", "colorOrder"],
        ["LEDPanelsBrightness", "brightness"],
        ["LEDPanelsGamma", "gamma"],
        ["LEDPanelsConnectionSelect", "subType"],
        ["LEDPanelsInterface", "interface"],
        ["LEDPanelsLayoutCols", "LEDPanelCols"],
        ["LEDPanelsLayoutRows", "LEDPanelRows"],
        ["LEDPanelsColorDepth", "panelColorDepth"],
        ["LEDPanelsStartCorner", "invertedData"],
        ["LEDPanelsRowAddressType", "panelRowAddressType"],
        ["LEDPanelsType", "panelType"],
        ["LEDPanelInterleave", "panelInterleave"],
        ["LEDPanelsOutputByRow", "panelOutputOrder", val => val ? 1 : 0],
        ["LEDPanelsOutputBlankRow", "panelOutputBlankRow", val => val ? 1 : 0],
        ["LEDPanelUIFrontView", "LEDPanelUIFrontView"],
        ["LEDPanelUIAdvancedLayout", "advanced"],
        ["LEDPanelsWiringPinout", "wiringPinout"],
        ["LEDPanelsGPIOSlowdown", "gpioSlowdown"],
        ["LEDPanelsEnabled", "enabled"],
        ["LEDPanelsOutputCPUPWM", "cpuPWM"],
        ["LEDPanelsColorOrder", "colorOrder"],
        ["LEDPanelMatrixName", "LEDPanelMatrixName"],
        ["LEDPanelsSize", "LEDPanelsSize"],
        ["LEDPanelsLayoutCols", "LEDPanelCols"],
        ["LEDPanelsLayoutRows", "LEDPanelRows"]
    ];

    function checkAndCorrectMissingChannelLookup() {
        if (typeof channelOutputsLookup === "undefined" || channelOutputsLookup === null || !channelOutputsLookup.LEDPanelMatrices || Object.keys(channelOutputsLookup.LEDPanelMatrices).length === 0) {
            {
                channelOutputsLookup = [];
                channelOutputsLookup["LEDPanelMatrices"] = [];
                channelOutputsLookup["LEDPanelMatrices"]["panelMatrix1"] = [];
                channelOutputsLookup["LEDPanelMatrices"]["panelMatrix1"].panelMatrixID = 1;
                SetDefaultsInChannelOutputsLookup();
            }
        }
    }


    function SetDefaultsInChannelOutputsLookup() {

        Object.values(channelOutputsLookup.LEDPanelMatrices).forEach(mp => {
            panelMatrixID = mp.panelMatrixID;
            mp.colorOrder ||= LEDPanelDefaults.LEDPanelColorOrder;
            mp.ledPanelsOutputs ||= LEDPanelDefaults.LEDPanelOutputs;
            mp.ledPanelsPanelsPerOutput ||= LEDPanelDefaults.LEDPanelsPanelsPerOutput;
            mp.LEDPanelAddressing ||= LEDPanelDefaults.LEDPanelAddressing;
            mp.gamma ||= LEDPanelDefaults.LEDPanelGamma;
            mp.ledPanelsLayout ||= LEDPanelDefaults.ledPanelsLayout;
            mp.LEDPanelsSize ||= LEDPanelDefaults.LEDPanelsSize;
            mp.gpioSlowdown ||= LEDPanelDefaults.gpioSlowdown;
            mp.wiringPinout ||= LEDPanelDefaults.LEDPanelsWiringPinout;


            const sizeparts = mp.LEDPanelsSize.split("x");
            // Assign values using destructuring
            [mp.panelWidth, mp.panelHeight, mp.panelScan, mp.LEDPanelAddressing] = sizeparts.map(Number);




            // Use optional chaining (`?.`) and default value to prevent errors
            const sizeParts = mp.ledPanelsLayout?.split("x") || [1, 1];
            mp.LEDPanelRows ||= parseInt(sizeParts[1], 10);
            mp.LEDPanelCols ||= parseInt(sizeParts[0], 10);

            //AutoLayoutPanels(panelMatrixID, 1);
        });
    }


    function UpdatePanelSize(panelMatrixID) {
        // Reference the panelMatrix object
        let mp = channelOutputsLookup.LEDPanelMatrices["panelMatrix" + panelMatrixID];

        const sizeparts = mp.LEDPanelsSize.split("x");
        // Assign values using destructuring
        [mp.panelWidth, mp.panelHeight, mp.panelScan, mp.LEDPanelAddressing] = sizeparts.map(Number);
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

        HandleChangesInUIValues();
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

    function RowAddressTypeChanged(panelMatrixID) {

        <? if (strpos($settings['SubPlatform'], 'PocketBeagle2') !== false) { ?>
            var value = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressType`).val());
            if (value >= 50) {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepthLabel`).hide();

                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).prop('checked', false);
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRowLabel`).hide();

            } else {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepthLabel`).show();

                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).show();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRowLabel`).show();
            }
            outputByRowClicked();
        <? } ?>
    }

    function checkInterleave(panelMatrixID) {
        let mp = channelOutputsLookup.LEDPanelMatrices?.["panelMatrix" + panelMatrixID];
        if ((mp.panelScan * 2) === mp.panelHeight) {
            $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).hide();
            $(`#panelMatrix${panelMatrixID} .LEDPanelInterleaveLabel`).hide();
        } else if (!(($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "ColorLight5a75") || ($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "X11PanelMatrix"))) {
            $(`#panelMatrix${panelMatrixID} .LEDPanelInterleave`).show();
            $(`#panelMatrix${panelMatrixID} .LEDPanelInterleaveLabel`).show();
        }
    }

    function UpdateLegacyLEDPanelLayout(panelMatrixID) {
        let mp = channelOutputsLookup.LEDPanelMatrices?.["panelMatrix" + panelMatrixID];
        if (!mp) {
            console.error("Invalid panelMatrixID or missing LEDPanelMatrices.");
            return;
        }

        mp.LEDPanelCols = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutCols`).val());
        mp.LEDPanelRows = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutRows`).val());

        UpdatePanelSize(panelMatrixID);

        const pixelCount = mp.LEDPanelCols * mp.LEDPanelRows * mp.panelWidth * mp.panelHeight;
        const channelCount = pixelCount * 3;

        $(`#panelMatrix${panelMatrixID} .LEDPanelsPixelCount`).html(pixelCount);
        $(`#panelMatrix${panelMatrixID} .LEDPanelsChannelCount`).html(channelCount);
        checkInterleave(panelMatrixID);
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


    function LEDPanelsLayoutChanged() {
        //get currently visible panelMatrixID
        panelMatrixID = GetCurrentActiveMatrixPanelID();


        var PanelLayoutValue = $(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutCols`).val() + "x" + $(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutRows`).val();

        var PanelLayoutUpdateJSONObj = {};
        PanelLayoutUpdateJSONObj.panelMatrices = {};
        PanelLayoutUpdateJSONObj.panelMatrices['panelMatrix' + panelMatrixID] = {};
        PanelLayoutUpdateJSONObj.panelMatrices['panelMatrix' + panelMatrixID].Configuration = {};
        PanelLayoutUpdateJSONObj.panelMatrices['panelMatrix' + panelMatrixID].Configuration.LEDPanelsLayout = {};

        PanelLayoutUpdateJSONObj.panelMatrices['panelMatrix' + panelMatrixID].Configuration.LEDPanelsLayout = PanelLayoutValue;

        var PanelLayoutValueJSON = JSON.stringify(PanelLayoutUpdateJSONObj);


        LEDPanelLayoutChanged();
        common_ViewPortChange();
    }


    function FrontBackViewToggled() {
        //get currently visible panelMatrixID
        const panelMatrixID = GetCurrentActiveMatrixPanelID();

        value = $(`#panelMatrix${panelMatrixID} .LEDPanelUIFrontView`).is(":checked");

        GetChannelOutputConfig(); // Refresh the in-memory config before redrawing
        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID]['LEDPanelUIFrontView'] = value;


        DrawLEDPanelTable(panelMatrixID);
        DisplaySaveWarningIfRequired();
    }

    function DrawLEDPanelTable(panelMatrixID) {
        var r;
        var c;
        var key = "";
        var frontView = 0;
        let mp = channelOutputsLookup.LEDPanelMatrices?.["panelMatrix" + panelMatrixID];

        var tbody = $(`#panelMatrix${panelMatrixID} .LEDPanelTable tbody`);
        tbody.empty();
        if ($(`#panelMatrix${panelMatrixID} .LEDPanelUIFrontView`).is(":checked")) {
            frontView = 1;
            tbody[0].insertRow(0).innerHTML = "<th colspan='" + mp.LEDPanelCols + "'>Front View</th>";
        } else {
            tbody[0].insertRow(0).innerHTML = "<th colspan='" + mp.LEDPanelCols + "'>Back View</th>";
        }

        for (r = 0; r < mp.LEDPanelRows; r++) {
            var html = "<tr>";
            for (i = 0; i < mp.LEDPanelCols; i++) {
                if (frontView)
                    c = i;
                else
                    c = mp.LEDPanelCols - 1 - i;

                html += "<td><table cellspacing=0 cellpadding=0><tr><td>";

                key = "LEDPanelOutputNumber_" + r + "_" + c;
                if (typeof mp[key] !== 'undefined') {
                    html += GetLEDPanelNumberSetting("O", key, mp.ledPanelsOutputs, mp[key].outputNumber);
                } else {
                    html += GetLEDPanelNumberSetting("O", key, mp.ledPanelsOutputs, 0);
                }

                html += "<img src='images/arrow_";

                if (typeof mp[key] !== 'undefined' && typeof mp[key].orientation !== 'undefined') {
                    html += mp[key].orientation;
                } else {
                    html += "N";
                }

                html += ".png' height=17 width=17 class='LEDPanelOrientation_" + r + "_" + c + "' onClick='LEDPanelOrientationClicked(\"LEDPanelOrientation_" + r + "_" + c + "\");'><br>";

                key = "LEDPanelPanelNumber_" + r + "_" + c;
                if (typeof mp[key] !== 'undefined') {
                    html += GetLEDPanelNumberSetting("P", key, mp.ledPanelsPanelsPerOutput, mp[key].panelNumber);
                } else {
                    html += GetLEDPanelNumberSetting("P", key, mp.ledPanelsPanelsPerOutput, 0);
                }
                html += "<br>";

                key = "LEDPanelColorOrder_" + r + "_" + c;
                if (typeof mp[key] !== 'undefined') {
                    html += GetLEDPanelColorOrder(key, mp[key].colorOrder);
                } else {
                    html += GetLEDPanelColorOrder(key, "");
                }

                html += "</td></tr></table></td>\n";
            }
            html += "</tr>";
            tbody[0].insertRow(-1).innerHTML = html;
        }
    }

    function isSpan(element) {
        if (element instanceof jQuery) {
            element = element[0]; // Convert jQuery object to DOM element
        }
        return element.tagName.toLowerCase() === "span";
    }



    function InitializeLEDPanelMatrix(panelMatrixID) {
        return new Promise((resolve) => {
            let mp = channelOutputsLookup?.LEDPanelMatrices?.["panelMatrix" + panelMatrixID];
            if (!mp || typeof mp !== "object") return;

            if (mp.enabled === 1 && Object.values(mp).includes("LEDPanelMatrix")) {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsEnabled`).prop("checked", true);
                $(`#panelMatrix${panelMatrixID} .tab-LEDPanels-LI`).show();
            }


            selectors.forEach(([selector, key, transform = val => val]) => {
                if (mp[key] != null) {
                    const $element = $(`#panelMatrix${panelMatrixID} .${selector}`);
                    if ($element.length > 0) {

                        if ($element.is(":checkbox")) {
                            $element.prop("checked", Boolean(transform(mp[key])));
                        } else if (isSpan($element)) {
                            $element.html(transform(mp[key]));
                        }
                        else {
                            $element.val(transform(mp[key]));
                        }
                    }

                }
            });

            <?php if ($settings['Platform'] == "Raspberry Pi" || $settings['BeaglePlatform']) { ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).val(mp?.wiringPinout);
            <?php } ?>

            <?php if ($settings['Platform'] == "Raspberry Pi") { ?>
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).val(mp?.gpioSlowdown);
            <?php } ?>

            const colordepth = mp?.panelColorDepth ?? 8;
            const outputByRow = mp?.panelOutputOrder ?? false;
            const outputBlank = mp?.panelOutputBlankRow ?? false;
            const invertedData = mp?.invertedData ?? 0;

            $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).val(colordepth);
            $(`#panelMatrix${panelMatrixID} .LEDPanelsStartCorner`).val(invertedData);

            if (mp.LEDPanelMatrixName != "") {
                $(`#matrixPanelTab${panelMatrixID} a`).html(mp.LEDPanelMatrixName);
            }

            PanelSubtypeChanged(panelMatrixID);
            // UpdateLegacyLEDPanelLayout(panelMatrixID);
            RowAddressTypeChanged(panelMatrixID);
            if (mp?.advanced == 1) {
                ToggleAdvancedLayout(panelMatrixID);
            }
            resolve();
        });
    }


    function GetLEDPanelConfigFromUI(panelMatrixID) {
        var config = new Object();
        var panels = "";
        var xOffset = 0;
        var yOffset = 0;
        var yDiff = 0;
        var advanced = 0;
        let mp = channelOutputsLookup.LEDPanelMatrices?.["panelMatrix" + panelMatrixID];

        var matrixDivName = 'panelMatrix' + panelMatrixID;
        var matrixDiv = $(`.tab-content [id=${matrixDivName}]`);

        config.cfgVersion = 3;
        config.type = "LEDPanelMatrix";
        if ((matrixDiv.find(`.LEDPanelUIAdvancedLayout`).is(":checked")) &&
            (typeof mp !== 'undefined')) {
            advanced = 1;
        }
        else {
            advanced = 0;
        }
        config.advanced = advanced;

        config.LEDPanelUIFrontView = matrixDiv.find('.LEDPanelUIFrontView').is(':checked');
        <?
        if ($panelCapesDriver != "") {
            echo "config.subType = '" . $panelCapesDriver . "';\n";
        } else if ($settings['BeaglePlatform']) {
            echo "config.subType = 'LEDscapeMatrix';\n";
        } else {
            echo "config.subType = 'RGBMatrix';\n";
        }
        if ($panelCapesName != "") {
            echo "config.configName = '" . $panelCapesName . "';\n";
        }
        ?>

        mp.LEDPanelsSize = matrixDiv.find('.LEDPanelsSize').val();
        UpdatePanelSize(panelMatrixID);
        config.ledPanelsWidth = mp.panelWidth;
        config.ledPanelsHeight = mp.panelHeight;
        config.ledPanelsScan = mp.panelScan;
        if (mp.LEDPanelAddressing) {
            config.panelAddressing = mp.LEDPanelAddressing;
        }

        config.panelMatrixID = panelMatrixID;
        config.enabled = Number(matrixDiv.find('.LEDPanelsEnabled').is(":checked"));
        config.LEDPanelsSize = matrixDiv.find('.LEDPanelsSize').val();
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

        config.LEDPanelMatrixName = matrixDiv.find('.LEDPanelMatrixName').val();
        config.LEDPanelRows = parseInt(matrixDiv.find('.LEDPanelsLayoutRows').val());
        config.LEDPanelCols = parseInt(matrixDiv.find('.LEDPanelsLayoutCols').val());
        config.ledPanelsLayout = matrixDiv.find('.LEDPanelsLayoutCols').val() + "x" + matrixDiv.find('.LEDPanelsLayoutRows').val();
        config.ledPanelsOutputs = parseInt(mp.ledPanelsOutputs);
        config.ledPanelsPanelsPerOutput = parseInt(mp.ledPanelsPanelsPerOutput);
        config.maxLEDPanels = mp.maxLEDPanels;
        config.panelColorDepth = parseInt(matrixDiv.find('.LEDPanelsColorDepth').val());
        config.gamma = matrixDiv.find('.LEDPanelsGamma').val();
        config.invertedData = parseInt(matrixDiv.find('.LEDPanelsStartCorner').val());
        config.panelWidth = mp.panelWidth;
        config.panelHeight = mp.panelHeight;
        config.panelScan = mp.panelScan;
        <? if ($settings['Platform'] == "Raspberry Pi" || $settings['BeaglePlatform']) { ?>
            config.panelOutputOrder = matrixDiv.find('.LEDPanelsOutputByRow').is(':checked');
            config.panelOutputBlankRow = matrixDiv.find('.LEDPanelsOutputBlankRow').is(':checked');
        <? } ?>
        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
            config.cpuPWM = matrixDiv.find('.LEDPanelsOutputCPUPWM').is(':checked');
        <? } ?>


        if (matrixDiv.find('.LEDPanelsRowAddressType').length > 0) {
            var rat = matrixDiv.find('.LEDPanelsRowAddressType').val();
            config.panelRowAddressType = parseInt(rat);
        }
        if (matrixDiv.find('.LEDPanelsType').length > 0) {
            var rat = matrixDiv.find('.LEDPanelsType').val();
            config.panelType = parseInt(rat);
        }

        if (matrixDiv.find('.LEDPanelInterleave').length > 0) {
            var rat = matrixDiv.find('.LEDPanelInterleave').val();
            config.panelInterleave = rat;
        }
        config.panels = [];

        if (matrixDiv.find('.LEDPanelsEnabled').is(":checked"))
            config.enabled = 1;

        if (advanced) {
            if (GetAdvancedPanelConfig(panelMatrixID).length > 0) {
                config.panels = GetAdvancedPanelConfig(panelMatrixID);
            }
            else { config.panels = mp.panels; }
        } else {
            for (r = 0; r < mp.LEDPanelRows; r++) {
                xOffset = 0;

                for (c = 0; c < mp.LEDPanelCols; c++) {
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
                        xOffset += mp.panelWidth;
                        yDiff = mp.panelHeight;
                    }
                    else if (src == 'images/arrow_R.png') {
                        panel.orientation = "R";
                        xOffset += mp.panelHeight;
                        yDiff = mp.panelWidth;
                    }
                    else if (src == 'images/arrow_U.png') {
                        panel.orientation = "U";
                        xOffset += mp.panelWidth;
                        yDiff = mp.panelHeight;
                    }
                    else if (src == 'images/arrow_L.png') {
                        panel.orientation = "L";
                        xOffset += mp.panelHeight;
                        yDiff = mp.panelWidth;
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
        global $SUDO;
        $interfaces = network_list_interfaces_array();
        foreach ($interfaces as $iface) {
            $iface = preg_replace("/:$/", "", $iface);
            echo "<option value='" . $iface;
            echo "'";
            if ($iface === "eth0") {
                echo " selected";
            }

            $ifaceSpeed = (int) exec("$SUDO ethtool $iface | grep -i 'baset' | grep -Eo '[0-9]{1,4}' | sort | tail -1");
            echo ">" . $iface . " (" . $ifaceSpeed . "Mbps)</option>";
        }
    }
    ?>

    function LEDPanelsConnectionChanged(panelMatrixID = GetCurrentActiveMatrixPanelID()) {
        return new Promise((resolve) => {

            const mp = channelOutputsLookup.LEDPanelMatrices?.["panelMatrix" + panelMatrixID];

            WarnIfSlowNIC(panelMatrixID);

            if (($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "ColorLight5a75") || ($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "X11PanelMatrix")) {
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdownLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepth`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsColorDepthLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinoutLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsWiringPinout`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressTypeLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressType`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsTypeLabel`).hide();
                $(`#panelMatrix${panelMatrixID} .LEDPanelsType`).hide();
                $(`#panelMatrix${panelMatrixID} .vendorPanelSettingsBtn`).hide();

                if ($(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`)[0].value === "X11PanelMatrix") {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionInterface`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightness`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightnessLabel`).hide();

                } else {
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionInterface`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightness`).show();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsBrightnessLabel`).show();
                }

                <?

                if ($settings['BeaglePlatform']) { ?>
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressTypeLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressType`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsTypeLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsType`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRowLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRowLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputBlankRow`).hide();
                <? } ?>


                <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWMLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWM`).hide();
                <? } ?>

                mp.ledPanelsOutputs = 24;
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
                $(`#panelMatrix${panelMatrixID} .vendorPanelSettingsBtn`).show();


                <? //NEEDS FIX
                if ($settings['BeaglePlatform']) { ?>
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
                        echo "        mp.ledPanelsOutputs = 5;\n";
                    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
                        echo "        mp.ledPanelsOutputs = 6;\n";
                    } else {
                        echo "        mp.ledPanelsOutputs = 8;\n";
                    }
                    ?> //////
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdownLabel`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsGPIOSlowdown`).hide();
                <? //NEEDS FIX
                } else {
                    if ($settings['Platform'] == "Raspberry Pi") { ?>
                        $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressTypeLabel`).show();
                        $(`#panelMatrix${panelMatrixID} .LEDPanelsRowAddressType`).show();
                        $(`#panelMatrix${panelMatrixID} .LEDPanelsTypeLabel`).show();
                        $(`#panelMatrix${panelMatrixID} .LEDPanelsType`).show();
                        $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWMLabel`).show();
                        $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputCPUPWM`).show();
                    <? }
                    ?> /////
                    mp.ledPanelsOutputs = 3;
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
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`).val(KNOWN_PANEL_CAPE["defaults"]["LEDPanelsConnection"]);
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionType`).hide();
                    $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionLabel`).hide();
                }
                mp.ledPanelsOutputs = KNOWN_PANEL_CAPE["outputs"].length;
            }

            PanelSubtypeChanged(panelMatrixID);
            DrawLEDPanelTable(panelMatrixID);
            resolve();
        });
    }

    <? if ($settings['BeaglePlatform']) { ?>
        function outputByRowClicked() {
            var checked = $(`#panelMatrix${panelMatrixID} .LEDPanelsOutputByRow`).is(':checked')
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

    //Define AdvancedUI canvas class and instantiate 5 objects (max number of panel matrices)

    class Canvas {
        constructor() {
            this.canvasInitialized = 0;
            this.uiScale = 3;
            this.panelGroups = [];
            this.ledPanelCanvas = [];
            this.selectedPanel = -1;
            this.canvasWidth = 900;
            this.canvasHeight = 400;
        }
    }

    const AdvancedUIcanvas1 = new Canvas();
    const AdvancedUIcanvas2 = new Canvas();
    const AdvancedUIcanvas3 = new Canvas();
    const AdvancedUIcanvas4 = new Canvas();
    const AdvancedUIcanvas5 = new Canvas();

    ///////////////
    function countChildObjects(obj) {
        let count = 0;

        if (typeof obj === "object" && obj !== null) {
            for (const key in obj) {
                if (typeof obj[key] === "object" && obj[key] !== null && !Array.isArray(obj[key])) {
                    count++; // Count only direct child objects
                }
            }
        }

        return count;
    }



    function GetAdvancedPanelConfig(panelMatrixID) {
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var panels = [];
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);

        let correctNumPanels = co.LEDPanelCols * co.LEDPanelRows;

        let currentCanvasPanelNum = countChildObjects(AdvancedUIcanvas.panelGroups);

        if (currentCanvasPanelNum === correctNumPanels) {
            for (var key in AdvancedUIcanvas.panelGroups) {
                var panel = new Object();
                var pg = AdvancedUIcanvas.panelGroups[key];

                var p = co.panels[pg.panelNumber];

                panel.outputNumber = p.outputNumber;
                panel.panelNumber = p.panelNumber;
                panel.xOffset = Math.round(pg.group.left / AdvancedUIcanvas.uiScale);
                panel.yOffset = Math.round(pg.group.top / AdvancedUIcanvas.uiScale);
                panel.orientation = p.orientation;
                panel.colorOrder = p.colorOrder;
                panel.row = p.row;
                panel.col = p.col;

                panels.push(panel);
            }
        }

        return panels;
    }

    function snapToGrid(panelMatrixID, o) {
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        var left = Math.round(o.left / AdvancedUIcanvas.uiScale);
        var top = Math.round(o.top / AdvancedUIcanvas.uiScale);
        var maxLeft = (AdvancedUIcanvas.canvasWidth / AdvancedUIcanvas.uiScale) - co.panelWidth;
        var maxTop = (AdvancedUIcanvas.canvasHeight / AdvancedUIcanvas.uiScale) - co.panelHeight;


        if ((o.orientation == 'R') ||
            (o.orientation == 'L')) {
            maxLeft = (AdvancedUIcanvas.canvasWidth / AdvancedUIcanvas.uiScale) - co.panelHeight;
            maxTop = (AdvancedUIcanvas.canvasHeight / AdvancedUIcanvas.uiScale) - co.panelWidth;
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
            left: left * AdvancedUIcanvas.uiScale,
            top: top * AdvancedUIcanvas.uiScale
        });
    }

    function panelMovingHandler(evt) {
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);

        var o = evt.target;
        var panels = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels;
        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var name = o.name;
        var outputNumber = o.outputNumber;
        var panelNumber = o.panelNumber;
        var colorOrder = o.colorOrder;

        snapToGrid(panelMatrixID, o);

        var left = Math.round(o.left / AdvancedUIcanvas.uiScale);
        var top = Math.round(o.top / AdvancedUIcanvas.uiScale);
        var desc = 'O-' + (outputNumber + 1) + ' P-' + (panelNumber + 1) + '\n@ ' + left + ',' + top;
        AdvancedUIcanvas.panelGroups[name].text.set('text', desc);

        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[AdvancedUIcanvas.panelGroups[name].panelNumber].xOffset = left;
        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[AdvancedUIcanvas.panelGroups[name].panelNumber].yOffset = top;

        panelSelectedHandler(evt);
        AdvancedUIcanvas.ledPanelCanvas.renderAll();

        UpdateMatrixSize(panelMatrixID);
    }

    function panelSelectedHandler(evt) {
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        var o = AdvancedUIcanvas.ledPanelCanvas.getActiveObject();

        if (AdvancedUIcanvas.selectedPanel != -1)
            AdvancedUIcanvas.panelGroups['panel-' + AdvancedUIcanvas.selectedPanel].panel.set({ 'stroke': 'black' });

        if (!o) {
            $(`#panelMatrix${panelMatrixID} .cpOutputNumber`).val(0);
            $(`#panelMatrix${panelMatrixID} .cpPanelNumber`).val(0);
            $(`#panelMatrix${panelMatrixID} .cpColorOrder`).val('');
            $(`#panelMatrix${panelMatrixID} .cpXOffset`).html('');
            $(`#panelMatrix${panelMatrixID} .cpYOffset`).html('');
            AdvancedUIcanvas.selectedPanel = -1;
        } else {
            $(`#panelMatrix${panelMatrixID} .cpOutputNumber`).val(o.outputNumber);
            $(`#panelMatrix${panelMatrixID} .cpPanelNumber`).val(o.panelNumber);
            $(`#panelMatrix${panelMatrixID} .cpColorOrder`).val(o.colorOrder);
            $(`#panelMatrix${panelMatrixID} .cpXOffset`).html(Math.round(o.left / AdvancedUIcanvas.uiScale));
            $(`#panelMatrix${panelMatrixID} .cpYOffset`).html(Math.round(o.top / AdvancedUIcanvas.uiScale));
            AdvancedUIcanvas.selectedPanel = AdvancedUIcanvas.panelGroups[o.name].panelNumber;
            AdvancedUIcanvas.panelGroups[o.name].panel.set({ 'stroke': 'rgb(255,255,0)' });
        }

        AdvancedUIcanvas.ledPanelCanvas.renderAll();
    }

    function RotateCanvasPanel() {
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        const selection = AdvancedUIcanvas.ledPanelCanvas?.getActiveObject();
        if (!selection || selection.type === "activeSelection") return;

        const panelNumber = AdvancedUIcanvas.panelGroups?.[selection.name]?.panelNumber;
        const panel = channelOutputsLookup?.LEDPanelMatrices?.["panelMatrix" + panelMatrixID]?.panels?.[panelNumber];
        if (!panel) return;

        // Update offsets
        panel.xOffset = Math.round(selection.left / AdvancedUIcanvas.uiScale);
        panel.yOffset = Math.round(selection.top / AdvancedUIcanvas.uiScale);

        // Rotate orientation using a mapping
        const rotationMap = { N: "R", R: "U", U: "L", L: "N" };
        panel.orientation = rotationMap[panel.orientation] || "N";

        SetupCanvasPanel(panelMatrixID, panelNumber);
        UpdateMatrixSize(panelMatrixID);
        //show hide changes warning
        DisplaySaveWarningIfRequired();
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
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);

        if ((typeof AdvancedUIcanvas.panelGroups !== 'undefined') && (name in AdvancedUIcanvas.panelGroups))
            AdvancedUIcanvas.ledPanelCanvas.remove(AdvancedUIcanvas.panelGroups[name].group);

        var w = co.panelWidth * AdvancedUIcanvas.uiScale;
        var h = co.panelHeight * AdvancedUIcanvas.uiScale;
        var a = 0;
        var tol = 0;
        var tot = 0;
        var aw = 9;
        var ah = 14;

        if ((p.orientation == 'R') ||
            (p.orientation == 'L')) {
            w = co.panelHeight * AdvancedUIcanvas.uiScale;
            h = co.panelWidth * AdvancedUIcanvas.uiScale;

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
            left: p.xOffset * AdvancedUIcanvas.uiScale,
            top: p.yOffset * AdvancedUIcanvas.uiScale,
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
        if (typeof AdvancedUIcanvas.panelGroups === 'undefined') AdvancedUIcanvas.panelGroups = [];
        AdvancedUIcanvas.panelGroups[name] = pg;

        AdvancedUIcanvas.ledPanelCanvas.add(group);
        AdvancedUIcanvas.ledPanelCanvas.setActiveObject(group);
        panelSelectedHandler();
    }

    function UpdateMatrixSize(panelMatrixID) {
        var matrixWidth = 0;
        var matrixHeight = 0;
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];

        for (var key in AdvancedUIcanvas.panelGroups) {
            var panel = new Object();
            var pg = AdvancedUIcanvas.panelGroups[key];

            if ((mp.panels[pg.panelNumber].orientation == 'N') ||
                (mp.panels[pg.panelNumber].orientation == 'U')) {
                if ((Math.round(pg.group.left / AdvancedUIcanvas.uiScale) + mp.panelWidth) > matrixWidth)
                    matrixWidth = Math.round(pg.group.left / AdvancedUIcanvas.uiScale) + mp.panelWidth;
                if ((Math.round(pg.group.top / AdvancedUIcanvas.uiScale) + mp.panelHeight) > matrixHeight)
                    matrixHeight = Math.round(pg.group.top / AdvancedUIcanvas.uiScale) + mp.panelHeight;
            } else {
                if ((Math.round(pg.group.left / AdvancedUIcanvas.uiScale) + mp.panelHeight) > matrixWidth)
                    matrixWidth = Math.round(pg.group.left / AdvancedUIcanvas.uiScale) + mp.panelHeight;
                if ((Math.round(pg.group.top / AdvancedUIcanvas.uiScale) + mp.panelWidth) > matrixHeight)
                    matrixHeight = Math.round(pg.group.top / AdvancedUIcanvas.uiScale) + mp.panelWidth;
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
            SetCanvasSize(panelMatrixID);
    }

    function SetCanvasSize(panelMatrixID = GetCurrentActiveMatrixPanelID()) {

        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);

        let canvasWidth = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsWide`).val()) * AdvancedUIcanvas.uiScale;
        let canvasHeight = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsHigh`).val()) * AdvancedUIcanvas.uiScale;

        AdvancedUIcanvas.ledPanelCanvas.setWidth(canvasWidth);
        AdvancedUIcanvas.ledPanelCanvas.setHeight(canvasHeight);
        AdvancedUIcanvas.ledPanelCanvas.calcOffset();
        AdvancedUIcanvas.ledPanelCanvas.renderAll();
    }

    function InitializeCanvas(panelMatrixID, reinit) {
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        AdvancedUIcanvas.canvasInitialized = 1;

        if (reinit) {
            AdvancedUIcanvas.selectedPanel = -1;
            for (var key in AdvancedUIcanvas.panelGroups) {
                AdvancedUIcanvas.ledPanelCanvas.remove(AdvancedUIcanvas.panelGroups[key].group);
            }
            AdvancedUIcanvas.panelGroups = [];
        } else {
            AdvancedUIcanvas.ledPanelCanvas = new fabric.Canvas(`ledPanelCanvas${panelMatrixID}`,
                {
                    backgroundColor: '#a0a0a0',
                    selection: false
                });
        }

        SetCanvasSize(panelMatrixID);

        if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].type === "LEDPanelMatrix") {
            for (var i = 0; i < channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels.length; i++) {
                SetupCanvasPanel(panelMatrixID, i);
            }
        }

        UpdateMatrixSize(panelMatrixID);

        AdvancedUIcanvas.ledPanelCanvas.on('object:moving', panelMovingHandler);
        AdvancedUIcanvas.ledPanelCanvas.on('selection:created', panelSelectedHandler);
        AdvancedUIcanvas.ledPanelCanvas.on('selection:updated', panelSelectedHandler);
        AdvancedUIcanvas.ledPanelCanvas.on('selection:cleared', panelSelectedHandler);
    }

    function ToggleAdvancedLayout(panelMatrixID = GetCurrentActiveMatrixPanelID()) {

        let mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);

        value = $(`#panelMatrix${panelMatrixID}  .LEDPanelUIAdvancedLayout`).is(":checked");

        mp.advanced = value ? 1 : 0;


        if (mp.advanced == 1) {

            $(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsWide`).val(mp.panelWidth * (mp.LEDPanelCols + 1));
            $(`#panelMatrix${panelMatrixID} .LEDPanelUIPixelsHigh`).val(mp.panelHeight * (mp.LEDPanelRows + 1));

            if (AdvancedUIcanvas.canvasInitialized)
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
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        if (AdvancedUIcanvas.selectedPanel < 0)
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[AdvancedUIcanvas.selectedPanel].outputNumber = parseInt($(`#panelMatrix${panelMatrixID} .cpOutputNumber`).val());
        SetupCanvasPanel(panelMatrixID, AdvancedUIcanvas.selectedPanel);
    }

    function cpPanelNumberChanged() {
        //get currently visible panelMatrixID
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        if (AdvancedUIcanvas.selectedPanel < 0)
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[AdvancedUIcanvas.selectedPanel].panelNumber = parseInt($(`#panelMatrix${panelMatrixID} .cpPanelNumber`).val());
        SetupCanvasPanel(panelMatrixID, AdvancedUIcanvas.selectedPanel);
    }

    function cpColorOrderChanged() {
        //get currently visible panelMatrixID
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let AdvancedUIcanvas = eval("AdvancedUIcanvas" + panelMatrixID);
        if (AdvancedUIcanvas.selectedPanel < 0)
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panels[AdvancedUIcanvas.selectedPanel].colorOrder = $(`#panelMatrix${panelMatrixID} .cpColorOrder`).val();
        SetupCanvasPanel(panelMatrixID, AdvancedUIcanvas.selectedPanel);
    }

    function SetupAdvancedUISelects(panelMatrixID) {
        let mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];


        for (var i = 0; i < mp.ledPanelsOutputs; i++) {
            $(`#panelMatrix${panelMatrixID} .cpOutputNumber`).append("<option value='" + i + "'>" + (i + 1) + "</option>");
        }

        for (var i = 0; i < mp.ledPanelsPanelsPerOutput; i++) {
            $(`#panelMatrix${panelMatrixID} .cpPanelNumber`).append("<option value='" + i + "'>" + (i + 1) + "</option>");
        }
    }

    function SetSinglePanelSize(panelMatrixID) {
        if (!("LEDPanelMatrices" in channelOutputsLookup))
            return;

        var co = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];

        var w = co.panelWidth;
        var h = co.panelHeight;
        var s = co.panelScan;
        var singlePanelSize = [w, h, s].join('x');
        if ("panelAddressing" in channelOutputsLookup) {
            singlePanelSize = [singlePanelSize, channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].panelAddressing].join('x');
        }
        $(`#panelMatrix${panelMatrixID} .LEDPanelsSize`).val(singlePanelSize.toString());
    }

    function GetCurrentActiveMatrixPanelID() {
        let element = $("[id^=panelMatrix].tab-pane.active")[0];

        return element && element.firstElementChild
            ? element.firstElementChild.innerText
            : "1"; // Default value if element not found
    }

    function PanelSubtypeChanged(panelMatrixID) {
        var select = document.getElementById("panelMatrix" + panelMatrixID).getElementsByClassName("printSettingFieldCola col-md-4 col-lg-4");
        var html = "";

        html += "<select class='LEDPanelsSize' onchange='LEDPanelsSizeChanged(GetCurrentActiveMatrixPanelID());'>"

        <? if ($settings['BeaglePlatform']) { ?>
            html += "<option value='32x16x8'>32x16 1/8 Scan</option>"
            html += "<option value='32x16x4'>32x16 1/4 Scan</option>"
            <? if (strpos($settings['SubPlatform'], 'PocketBeagle2') !== false) { ?>
                html += "<option value='32x16x2'>32x16 1/2 Scan</option>"
            <? } else { ?>
                html += "<option value='32x16x4x1'>32x16 1/4 Scan ABCD</option>"
                html += "<option value='32x16x2'>32x16 1/2 Scan A</option>"
                html += "<option value='32x16x2x1'>32x16 1/2 Scan AB</option>"
            <? } ?>
            html += "<option value='32x32x16'>32x32 1/16 Scan</option>"
            html += "<option value='32x32x8'>32x32 1/8 Scan</option>"
            html += "<option value='64x32x16'>64x32 1/16 Scan</option>"
            html += "<option value='64x32x8'>64x32 1/8 Scan</option>"
            html += "<option value='64x64x16'>64x64 1/16 Scan</option>"
            <? if ($panelCapesHaveSel4) { ?>
                html += "<option value='64x64x32'>64x64 1/32 Scan</option>"
                html += "<option value='128x64x16'>128x64 1/16 Scan</option>"
                html += "<option value='128x64x32'>128x64 1/32 Scan</option>"
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
            if ($(`#panelMatrix${panelMatrixID}` + " .LEDPanelsConnectionSelect").val() === 'ColorLight5a75') {
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

    function AutoLayoutPanels(panelMatrixID = GetCurrentActiveMatrixPanelID(), override = 0) {
        mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];

        if (override == 0) {
            var sure = prompt('WARNING: This will re-configure all output and panel numbers.  Are you sure? [Y/N]', 'N');
        } else
            var sure = 'Y';
        if (sure.toUpperCase() != 'Y')
            return;

        var panelsWide = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutCols`).val());
        var panelsHigh = parseInt($(`#panelMatrix${panelMatrixID} .LEDPanelsLayoutRows`).val());

        //Delete existing panel data from channelOutputsLookup
        // Loop through legacy object keys and delete matching ones
        Object.keys(mp).forEach(key => {
            if (key.startsWith("LEDPanelPanelNumber_") || key.startsWith("LEDPanelOutputNumber_") || key.startsWith("LEDPanelOrientation_") || key.startsWith("LEDPanelColorOrder_")) {
                delete mp[key];
            }
        });
        //reset advanced panels
        mp.panels = [];


        for (var y = 0; y < panelsHigh; y++) {
            for (var x = 0; x < panelsWide; x++) {
                var panel = panelsWide - x - 1;
                //Update display
                $(`#panelMatrix${panelMatrixID} .LEDPanelPanelNumber_${y}_${panel}`).val(x);
                $(`#panelMatrix${panelMatrixID} .LEDPanelOutputNumber_${y}_${panel}`).val(y);
                $(`#panelMatrix${panelMatrixID} .LEDPanelOrientation_${y}_${panel}`).attr('src', 'images/arrow_N.png');
                $(`#panelMatrix${panelMatrixID} .LEDPanelColorOrder_${y}_${panel}`).val('');
                //Update channelOutputsLookup
                // Ensure the legacy objects exist before setting properties
                mp["LEDPanelPanelNumber_" + y + "_" + panel] = mp["LEDPanelPanelNumber_" + y + "_" + panel] || {};
                mp["LEDPanelOutputNumber_" + y + "_" + panel] = mp["LEDPanelOutputNumber_" + y + "_" + panel] || {};
                mp["LEDPanelOrientation_" + y + "_" + panel] = mp["LEDPanelOrientation_" + y + "_" + panel] || {};


                mp["LEDPanelPanelNumber_" + y + "_" + panel].panelNumber = x;
                mp["LEDPanelOutputNumber_" + y + "_" + panel].outputNumber = y;
                mp["LEDPanelOrientation_" + y + "_" + panel].orientation = 'N';
                //update advanced panels config
                //var newPanel = []
                //mp.panels
            }
        }
        //SaveChannelOutputsJSON();
    }


    function TogglePanelTestPattern() {

        panelMatrixID = GetCurrentActiveMatrixPanelID();
        var val = $("#PanelTestPatternButton").val();
        if (val == "Test Pattern") {
            var outputType = $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`).val();
            <?
            if ($panelCapesDriver == "BBShiftPanel") {
                echo "outputType = 'BB64 Panels';";
            } else {
                ?>
                if (outputType == "ColorLight5a75") {
                    outputType = "ColorLight Panels";
                } else if (outputType == "RGBMatrix") {
                    outputType = "Pi Panels";
                } else if (outputType == "BBBMatrix" || outputType == "LEDscapeMatrix") {
                    outputType = "BBB Panels";
                }
                <?
            }
            ?>



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

    function findNextAvailableId(obj) {
        const usedIds = Object.keys(obj).map(key => parseInt(key.replace("panelMatrix", ""), 10));
        for (let i = 1; i <= 5; i++) {
            if (!usedIds.includes(i)) {
                return i; // Return the first missing number
            }
        }
        return usedIds.length + 1; // If no gaps, return the next number in sequence
    }


    function AddPanelMatrixDialog() {

        var mp = channelOutputsLookup["LEDPanelMatrices"];

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
                    id: 'AddPanelButton',
                    class: 'btn-danger',
                    click: function () {

                        //get next panel matrix ID
                        var NewPanelMatrixID = findNextAvailableId(channelOutputsLookup["LEDPanelMatrices"]);
                        if (NewPanelMatrixID > 5) {
                            alert("Maximum number of panel matrices reached.  Please remove a panel matrix before adding a new one.");
                            return;
                        }

                        //add new matrix to channelOutputsLookup
                        //set the default values for the new panel matrix based on system defaults
                        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + NewPanelMatrixID] = {
                            type: "LEDPanelMatrix",
                            panelMatrixID: NewPanelMatrixID,
                            panels: [],
                            enabled: true,
                            invertedData: LEDPanelDefaults['invertedData'],
                            LEDPanelsSize: LEDPanelDefaults['LEDPanelsSize'],
                            LEDPanelsOutputByRow: LEDPanelDefaults['LEDPanelsOutputByRow'],
                            panelOutputBlankRow: LEDPanelDefaults['LEDPanelsOutputBlankRow'],
                            LEDPanelsLayoutCols: LEDPanelDefaults['LEDPanelsLayoutCols'],
                            LEDPanelsLayoutRows: LEDPanelDefaults['LEDPanelsLayoutRows'],
                            panelWidth: LEDPanelDefaults['LEDPanelWidth'],
                            panelHeight: LEDPanelDefaults['LEDPanelHeight'],
                            panelScan: LEDPanelDefaults['LEDPanelScan'],
                            colorOrder: LEDPanelDefaults['LEDPanelColorOrder'],
                            gamma: LEDPanelDefaults['LEDPanelGamma'],
                            LEDPanelCols: LEDPanelDefaults['LEDPanelCols'],
                            LEDPanelRows: LEDPanelDefaults['LEDPanelRows'],
                            ledPanelsLayout: LEDPanelDefaults['ledPanelsLayout'],
                            advanced: LEDPanelDefaults['LEDPanelUIAdvancedLayout'],
                            LEDPanelUIFrontView: LEDPanelDefaults['LEDPanelUIFrontView'],
                            ledPanelsPanelsPerOutput: LEDPanelDefaults['LEDPanelsPanelsPerOutput'],
                            ledPanelsOutputs: LEDPanelDefaults['LEDPanelOutputs'],
                            colorOrder: LEDPanelDefaults['LEDPanelColorOrder'],
                            panelScan: LEDPanelDefaults['LEDPanelScan'],
                            panelType: LEDPanelDefaults['LEDPanelType']
                        }
                        //if type is colorlight, set the defaults
                        if ($(`#AddPanelMatrixDialog #LEDPanelsConnectionSelect`).val() == "ColorLight5a75") {
                            Object.assign(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + NewPanelMatrixID], {
                                subType: "ColorLight5a75"
                            });
                        }
                        if ($(`#AddPanelMatrixDialog #LEDPanelsConnectionSelect`).val() == "RGBMatrix") {
                            Object.assign(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + NewPanelMatrixID], {
                                subType: "RGBMatrix",
                                wiringPinout: LEDPanelDefaults['LEDPanelsWiringPinout'],
                                gpioSlowdown: LEDPanelDefaults['gpioSlowdown'],
                                cpuPWM: LEDPanelDefaults["LEDPanelsOutputCPUPWM"],
                                LEDPanelAddressing: LEDPanelDefaults['LEDPanelAddressing']
                            });
                        };
                        if ($(`#AddPanelMatrixDialog #LEDPanelsConnectionSelect`).val() == "BBBMatrix") {
                            Object.assign(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + NewPanelMatrixID], {
                                subType: "BBBMatrix",
                                wiringPinout: LEDPanelDefaults['LEDPanelsWiringPinout'],
                                gpioSlowdown: LEDPanelDefaults['gpioSlowdown'],
                                cpuPWM: LEDPanelDefaults["LEDPanelsOutputCPUPWM"],
                                LEDPanelAddressing: LEDPanelDefaults['LEDPanelAddressing']
                            });
                        };
                        if ($(`#AddPanelMatrixDialog #LEDPanelsConnectionSelect`).val() == "LEDscapeMatrix") {
                            Object.assign(channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + NewPanelMatrixID], {
                                subType: "LEDscapeMatrix",
                                wiringPinout: LEDPanelDefaults['LEDPanelsWiringPinout'],
                                gpioSlowdown: LEDPanelDefaults['gpioSlowdown'],
                                cpuPWM: LEDPanelDefaults["LEDPanelsOutputCPUPWM"],
                                LEDPanelAddressing: LEDPanelDefaults['LEDPanelAddressing']
                            });
                        };


                        CloseModalDialog('AddPanelMatrixDialog');

                        populatePanelMatrixTab(NewPanelMatrixID).then(() => {
                            return InitializeLEDPanelMatrix(NewPanelMatrixID);
                        }).then(() => { return LEDPanelsConnectionChanged(NewPanelMatrixID); });

                        //populatePanelMatrixTab(NewPanelMatrixID);
                        //InitializeLEDPanelMatrix(NewPanelMatrixID);
                        //LEDPanelsConnectionChanged(NewPanelMatrixID);
                        SetupAdvancedUISelects(NewPanelMatrixID);
                        SetSinglePanelSize(NewPanelMatrixID);
                        AutoLayoutPanels(NewPanelMatrixID, 1);
                        SetupToolTips();
                        //location.reload();
                        //show tab of new panel matrix
                        //set the new tab to be active 
                        $(`#matrixPanelTab${NewPanelMatrixID} a`).tab('show');


                    }
                },
                Abort: {
                    disabled: false,
                    id: 'AbortButton',
                    click: function () {
                        CloseModalDialog('AddPanelMatrixDialog');
                        //location.reload();
                    }
                }
            },
            open: function () {
                //set available connections based on existing configName
                // Create an object to track subtype counts
                let subtypeCount = {};

                Object.values(mp).forEach(item => {
                    if (item.subType) { // Ensure subType exists
                        subtypeCount[item.subType] = (subtypeCount[item.subType] || 0) + 1;
                    }
                });

                //Disable subtypes that are already in use based on maxes
                if (subtypeCount["ColorLight5a75"] >= 4) {
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='ColorLight5a75']").prop("disabled", true);
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='ColorLight5a75']").text(function (_, currentText) {
                        return currentText + " (max number reached)";
                    });
                }
                if (subtypeCount["RGBMatrix"] >= 1) {
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='RGBMatrix']").prop("disabled", true);
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='RGBMatrix']").text(function (_, currentText) {
                        return currentText + " (max number reached)";
                    });
                }
                if (subtypeCount["BBBMatrix"] >= 1) {
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='BBBMatrix']").prop("disabled", true);
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='BBBMatrix']").text(function (_, currentText) {
                        return currentText + " (max number reached)";
                    });
                }
                if (subtypeCount["LEDscapeMatrix"] >= 1) {
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='LEDscapeMatrix']").prop("disabled", true);
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option[value='LEDscapeMatrix']").text(function (_, currentText) {
                        return currentText + " (max number reached)";
                    });
                }

                //default to select first non disabled option:
                let firstEnabledOption = $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect option:not(:disabled)").first();
                if (firstEnabledOption.length) {
                    $("#AddPanelMatrixDialog #LEDPanelsConnectionSelect").val(firstEnabledOption.val());
                }

            }

        };

        //copy the HTML from the template
        options.body = document.querySelector("#AddPanelDialogCode").innerHTML;

        DoModalDialog(options);
    }

    function MatrixNameChange() {
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        let newName = $(`#panelMatrix${panelMatrixID} .LEDPanelMatrixName`).val();
        if (newName !== "") {
            $(`#matrixPanelTab${panelMatrixID} a`).html(newName);
        } else {
            $(`#matrixPanelTab${panelMatrixID} a`).html("Panel Matrix " + panelMatrixID);
        }
    }

    function WarnIfSlowNIC(panelMatrixID = GetCurrentActiveMatrixPanelID()) {
        var txt = $(`#panelMatrix${panelMatrixID} .LEDPanelsInterface`).find(":selected").text();
        var NicSpeed = 1000;
        if (txt != "") {
            parseInt(txt.split('(')[1].split('M')[0]);
        }
        if (NicSpeed < 1000 && $(`#panelMatrix${panelMatrixID} .LEDPanelsConnectionSelect`).find(":selected").text() == "ColorLight" && $(`#panelMatrix${panelMatrixID} .LEDPanelsEnabled`).is(":checked") == true) {
            $(`#panelMatrix${panelMatrixID} .divLEDPanelWarnings`).html(`<div class="alert alert-danger">Selected interface for Panel Matrix ${panelMatrixID} does not support 1000+ Mbps, which is the Colorlight minimum</div>`);
        } else {
            $(`#panelMatrix${panelMatrixID} .divLEDPanelWarnings`).html("");
        }
    }

    function LEDPanelsSizeChanged(panelMatrixID) {
        var value = $(`#panelMatrix${panelMatrixID} .LEDPanelsSize`).val();
        //SetSetting("LEDPanelsSize", value, 1, 0, false, null, function () {
        //settings['LEDPanelsSize'] = value;
        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID]['LEDPanelsSize'] = value;
        LEDPanelLayoutChanged();

    }

    function populatePanelMatrixTab(panelMatrixID) {
        checkAndCorrectMissingChannelLookup();
        return new Promise((resolve) => {
            //copy from template
            document.querySelector(`#panelMatrix${panelMatrixID}`).innerHTML = document.querySelector('#divLEDPanelsTemplate').innerHTML;
            //set the tab to be visible
            document.querySelector(`#matrixPanelTab${panelMatrixID}`).style.display = "block";
            //Set the PanelMatrixID
            document.querySelector(`#panelMatrix${panelMatrixID}  .divPanelMatrixID`).innerHTML = panelMatrixID;
            $(`#panelMatrix${panelMatrixID}  .matrixPanelID`).html(panelMatrixID);
            //Rename the Canvas id
            $(`#panelMatrix${panelMatrixID} canvas.ledPanelCanvas`).attr("id", "ledPanelCanvas" + panelMatrixID);
            //Set which elements are visible on first render
            if (channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID].advanced == 1) {
                $(`#panelMatrix${panelMatrixID} .ledPanelCanvasUI`).show();
                $(`#panelMatrix${panelMatrixID} .ledPanelSimpleUI`).hide();
            } else {
                $(`#panelMatrix${panelMatrixID} .ledPanelCanvasUI`).hide();
                $(`#panelMatrix${panelMatrixID} .ledPanelSimpleUI`).show();
            }
            //resolve the promise after the DOM is updated
            resolve();
        });

    }

    function RemovePanelMatrixConfig() {
        //get currently visible panelMatrixID
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        var sure = prompt('WARNING: This will remove the LED Panel Matrix configuration.  Are you sure? [Y/N]', 'N');

        if (sure.toUpperCase() != 'Y')
            return;

        delete channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        document.querySelector(`#panelMatrix${panelMatrixID}`).innerHTML = "No Panels Defined - Use \"Add Panel Matrix\" button";
        $(`#matrixPanelTab${panelMatrixID}`).hide();

        SetMatrixDisplayDefault();

    }

    function SetMatrixDisplayDefault() {
        // Ensure LEDPanelMatrices exists and is not empty
        if (Array.isArray(channelOutputsLookup.LEDPanelMatrices) && Object.keys(channelOutputsLookup.LEDPanelMatrices).length > 0) {
            // Set first tab to be active
            let objkey = Object.keys(channelOutputsLookup.LEDPanelMatrices).reduce((min, current) =>
                current.panelMatrixID < min.panelMatrixID ? current : min
            );

            let objkeyNum = objkey.replace("panelMatrix", "");
            $(`#matrixPanelTab${objkeyNum} a`).tab('show');
        } else {
            console.warn("No LED panel matrices found. Defaulting to show empty panel matrix 1 tab");
            $(`#matrixPanelTab1 a`).tab('show');
        }
    }

    function sortJson(obj) {
        if (Array.isArray(obj)) {
            return obj.map(sortJson).sort((a, b) => JSON.stringify(a).localeCompare(JSON.stringify(b)));
        } else if (typeof obj === "object" && obj !== null) {
            return Object.keys(obj)
                .sort()
                .reduce((sortedObj, key) => {
                    sortedObj[key] = sortJson(obj[key]);
                    return sortedObj;
                }, {});
        } else {
            return obj;
        }
    }

    function DetectConfigChangesInUI() {

        //get currently visible panelMatrixID
        const panelMatrixID = GetCurrentActiveMatrixPanelID();

        // Find the matching object from saved config
        let resultObject = structuredClone(channelOutputs.channelOutputs.find(obj => obj.panelMatrixID === panelMatrixID) || {});
        ;
        // Sort object keys alphabetically
        const sortedSavedObj = sortJson(resultObject);

        //The current channeloutputsLookup for current panelMatrixID
        let currentConfigObj = structuredClone(channelOutputsLookup.LEDPanelMatrices[`panelMatrix${panelMatrixID}`] || {});

        //remove the elements used only for the UI which are not in the saved config
        Object.keys(currentConfigObj).forEach(key => {
            if (key.startsWith("LEDPanelOutputNumber_") ||
                key.startsWith("LEDPanelPanelNumber_") ||
                key.startsWith("LEDPanelOrientation_") ||
                key.startsWith("LEDPanelColorOrder_")
                <? if ($settings['Platform'] != "Raspberry Pi") { ?>
                    || key.startsWith("gpioSlowdown")
                <? } ?>

            ) {
                delete currentConfigObj[key]; // Remove the property
            }
        });

        // Sort object keys alphabetically
        const sortedCurrentObj = sortJson(currentConfigObj);

        // Compare objects and log differences
        let differences = [];
        Object.keys({ ...sortedSavedObj, ...sortedCurrentObj }).forEach(key => {
            let saved = sortedSavedObj[key];
            let current = sortedCurrentObj[key];
            if (Array.isArray(saved)) {
                // kind of hacky...
                let savedJSON = JSON.stringify(saved);
                let currentJSON = JSON.stringify(current);
                if (savedJSON !== currentJSON) {
                    differences.push({
                        key: key,
                        savedValue: saved,
                        currentValue: current
                    });
                }
            } else if (saved !== current) {
                differences.push({
                    key: key,
                    savedValue: sortedSavedObj[key],
                    currentValue: sortedCurrentObj[key]
                });
            }
        });

        if (differences.length > 0) {
            console.log("Differences detected:", differences);
            return true;
        } else {
            console.log("No differences found.");
            return false;
        }
    }

    function HandleChangesInUIValues() {
        const panelMatrixID = GetCurrentActiveMatrixPanelID();
        //Update the channelOutputsLookup with the new values from UI screen
        let currentUIConfig = GetLEDPanelConfigFromUI(panelMatrixID);
        channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID] = currentUIConfig;
        mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        //re-add derived values
        for (var p = 0; p < mp.panels.length; p++) {
            var r = mp.panels[p].row;
            var c = mp.panels[p].col;

            mp["LEDPanelOutputNumber_" + r + "_" + c] = mp.panels[p];
            mp["LEDPanelPanelNumber_" + r + "_" + c] = mp.panels[p];
            mp["LEDPanelColorOrder_" + r + "_" + c] = mp.panels[p];
        }

        //show hide changes warning
        DisplaySaveWarningIfRequired();
    }


    $(document).ready(function () {

        <?
        //Discover currently configured panel matrices and populate a tab and initialize each        
        for ($z = 0; $z < count($matricesArray); $z++) {
            $panelMatrixID = $matricesArray[$z]["panelMatrixID"] ?? 1;
            //set whether the tabs are displayed
            echo "$('#matrixPanelTab$panelMatrixID').show();\n";
            //populate the tab
            echo "populatePanelMatrixTab($panelMatrixID).then(() => {
            return InitializeLEDPanelMatrix($panelMatrixID);
            }).then(() => {
            checkInterleave($panelMatrixID);
            return LEDPanelsConnectionChanged($panelMatrixID);});\n";
            echo "SetupAdvancedUISelects($panelMatrixID);\n";
        }

        ?>


        <?
        if (
            (isset($settings['cape-info'])) &&
            ((in_array('panels', $currentCapeInfo["provides"])))
        ) {
            ?>
            if (currentCapeName != "" && currentCapeName != "Unknown") {
                $('.capeNamePanels').html(currentCapeName);
            }

            <?
        }
        ?>
        WarnIfSlowNIC(1);
        SetupToolTips();

    });


    $(document).on("change input", "#divLEDPanelMatrices select, #divLEDPanelMatrices input, #divLEDPanelMatrices textarea", function (event) {
        if ($(this).is("input[type='text'], textarea") && event.type === "change") return;
        console.log(`Changed: ${$(this).attr("class")} -> ${this.value}`);

        HandleChangesInUIValues();

    });

    $(document).on("shown.bs.tab", 'a[data-bs-toggle="pill"]', function (event) {
        if (event.target.id.startsWith("tab-LEDPanels-tab")) {
            //Default to first tab if no tab is selected
            SetMatrixDisplayDefault();
        }
        //console.log("Tab activated:", event.target); // Logs the activated pill
    });

    var lastVendorPanelSelect = new Array();
    function PanelTypeSelected() {
        $('#vendorPanelSelectDialog .vendorPanelURL').html("<br><p></p>");
        var panel = $('#vendorPanelSelectDialog .selectPanel').val();
        Object.entries(lastVendorPanelSelect).forEach(function ([idx, details]) {
            if (details["name"] == panel) {
                if (details["url"] !== undefined) {
                    var html = `<br><p>Link: <a href="${details["url"]}" target="_blank">${details["url"]}</a></p>`;
                    $('#vendorPanelSelectDialog .vendorPanelURL').html(html);
                }
            }
        });
    }
    function PopulatePanelTypes() {
        var url = $('#vendorPanelSelectDialog .selectVendor').val();
        $('#vendorPanelSelectDialog .selectPanel').empty();
        if (url === "") {
            return;
        }
        var request = new XMLHttpRequest();
        request.open('GET', url, true);
        request.overrideMimeType("application/json");
        request.onload = function () {
            const panelMatrixID = GetCurrentActiveMatrixPanelID();
            let mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
            lastVendorPanelSelect = JSON.parse(request.responseText);
            Object.entries(lastVendorPanelSelect).forEach(function ([idx, details]) {
                if (details["settings"]["all"] !== undefined || details) {
                    $('#vendorPanelSelectDialog .selectPanel').append(
                        `<option value="${details['name']}">${details['name']}</option>`
                    );
                }
            });
            PanelTypeSelected();
        };
        request.send();
    }
    function applyPanelProperties(panelMatrixID, details) {
        let mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
        var matrixDivName = 'panelMatrix' + panelMatrixID;
        var matrixDiv = $(`.tab-content [id=${matrixDivName}]`);

        Object.entries(details).forEach(function ([name, value]) {
            console.log("Applying panel properties for " + name + " with value: " + value);
            if (name == "panelWidth" || name == "panelHeight" || name == "panelScan") {
                // these three must always be set together
                var val = details["panelWidth"] + "x" + details["panelHeight"] + "x" + details["panelScan"];
                $(`#${matrixDivName} .LEDPanelsSize`).val(val);
                LEDPanelsSizeChanged(panelMatrixID);
            } else if (name == "panelOutputOrder") {
                mp.LEDPanelsOutputByRow = value;
                $(`#${matrixDivName} .LEDPanelsOutputByRow`).prop("checked", value);
                outputByRowClicked();
            } else if (name == "panelOutputBlankRow") {
                mp.panelOutputBlankRow = value;
                $(`#${matrixDivName} .LEDPanelsOutputBlankRow`).prop("checked", value);
            } else if (name == "panelInterleave") {
                mp.ledPanelsInterleave = value;
                $(`#${matrixDivName} .LEDPanelInterleave`).val(value);
                LEDPanelLayoutChanged();
            } else if (name == "panelColorOrder") {
                mp.colorOrder = value;
                $(`#${matrixDivName} .LEDPanelsColorOrder`).val(value);
            } else if (name == "panelAddressing") {
                mp.panelAddressing = value;
                $(`#${matrixDivName} .LEDPanelsRowAddressType`).val(value);
                RowAddressTypeChanged(panelMatrixID);
            } else if (name == "panelType") {
                mp.panelType = value;
                $(`#${matrixDivName} .LEDPanelsType`).val(value);
            }
        });
    }
    $(function () {
        const url = "https://raw.githubusercontent.com/FalconChristmas/fpp-data/refs/heads/master/panels/fpp-panels.json";
        var request = new XMLHttpRequest();
        request.open('GET', url, true);
        request.overrideMimeType("application/json");
        request.onload = function () {
            var jsonResponse = JSON.parse(request.responseText);
            Object.entries(jsonResponse["vendors"]).forEach(function ([vendor, details]) {
                $('#vendorPanelSelectDialog .selectVendor').append(
                    `<option value="${details['url']}">${vendor}</option>`
                );
            });
        };
        request.send();
        $('.vendorPanelSettingsBtn').on("click", function () {
            const panelMatrixID = GetCurrentActiveMatrixPanelID();
            let mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];

            DoModalDialog({
                id: "PanelSelectDialog",
                backdrop: true,
                keyboard: true,
                title: "Select Vendor and Panel Type",
                body: $("#vendorPanelSelectDialog"),
                class: "modal-m",
                buttons: {
                    "Select": {
                        click: function () {
                            var panel = $('#vendorPanelSelectDialog .selectPanel').val();
                            const panelMatrixID = GetCurrentActiveMatrixPanelID();
                            Object.entries(lastVendorPanelSelect).forEach(function ([idx, details]) {
                                if (details["name"] == panel) {
                                    let mp = channelOutputsLookup["LEDPanelMatrices"]["panelMatrix" + panelMatrixID];
                                    if (details["settings"]["all"] != undefined) {
                                        applyPanelProperties(panelMatrixID, details["settings"]["all"]);
                                    }
                                    if (details["settings"][mp.subType] != undefined) {
                                        applyPanelProperties(panelMatrixID, details["settings"][mp.subType]);
                                    }
                                }
                            });
                            CloseModalDialog("PanelSelectDialog");
                        },
                        class: 'btn-success'
                    },
                    "Cancel": function () {
                        CloseModalDialog("PanelSelectDialog");
                    }
                }
            });
        });
    });

</script>
<div id="divLEDPanelMatrices">
    <div class="row tableTabPageHeader">
        <div class="col-md">
            <h2><span class='capeNamePanels'>LED Panel Matrices</span> </h2>
        </div>
        <div class="col-md-auto ms-lg-auto">
            <div class="form-actions">
                <button id="AddPanelMatrixButton" class="buttons btn-outline-success btn-rounded  btn-icon-add"
                    onClick='AddPanelMatrixDialog();'><i class="fas fa-plus"></i> Add Panel Matrix
                </button>
                <input id="PanelTestPatternButton" type='button' class="buttons ms-1"
                    onClick='TogglePanelTestPattern();' value='Test Pattern'>
                <input type='button' class="buttons btn-success ms-1" onClick='SaveChannelOutputsJSON();' value='Save'>
            </div>
        </div>
        <div id="SaveChangeWarningLabel" class="alert alert-danger" style="display:none;">
            <strong>Warning!</strong> You have unsaved changes. Please save your changes before leaving this page.
        </div>
    </div>
    <!-- LED Panel Matrix Tabs --->
    <ul class="nav nav-tabs" id="panelTabs" role="tablist">
        <li class="nav-item" style="display: none;" id="matrixPanelTab1"><a class="nav-link active" href="#panelMatrix1"
                data-bs-toggle="tab" data-bs-target="#panelMatrix1">Panel Matrix 1</a></li>
        <li class="nav-item" style="display: none;" id="matrixPanelTab2"><a class="nav-link" href="#panelMatrix2"
                data-bs-toggle="tab" data-bs-target="#panelMatrix2">Panel Matrix 2</a></li>
        <li class="nav-item" style="display: none;" id="matrixPanelTab3"><a class="nav-link" href="#panelMatrix3"
                data-bs-toggle="tab" data-bs-target="#panelMatrix3">Panel Matrix 3</a></li>
        <li class="nav-item" style="display: none;" id="matrixPanelTab4"><a class="nav-link" href="#panelMatrix4"
                data-bs-toggle="tab" data-bs-target="#panelMatrix4">Panel Matrix 4</a></li>
        <li class="nav-item" style="display: none;" id="matrixPanelTab5"><a class="nav-link" href="#panelMatrix5"
                data-bs-toggle="tab" data-bs-target="#panelMatrix5">Panel Matrix 5</a></li>
    </ul>



    <!-- LED Panel Matrix Tab-Content --->

    <div class="panelMatrix-tab-content tab-content">
        <div class="tab-pane active" id="panelMatrix1">No Panels Defined - Use "Add Panel Matrix" button</div>
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
            <li>When wiring panels, divide the panels across as many outputs as possible. Shorter chains on more outputs
                will have higher refresh than longer chains on fewer outputs.</li>
            <li>If not using all outputs, use all the outputs from 1 up to what is needed. Data is always sent on
                outputs up to the highest configured, even if no panels are attached.</li>
            <? if ($settings['Platform'] == "Raspberry Pi") { ?>
            <li>If <b>only one single panel works</b> and everything else is correctly configured, try one of the
                special "LED Panel Type" options!</li>
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
            <div class="divLEDPanelHeader backdrop row">
                <div class="col-md-auto form-inline">
                    <div class="backdrop-dark form-inline enableCheckboxWrapper">
                        <div><b>Enable LED Panel Matrix:&nbsp; <span class='matrixPanelID'></span></b></div>
                        <div><input class='LEDPanelsEnabled' type='checkbox' onChange="WarnIfSlowNIC();"></div>

                    </div>
                </div>
                <div class="col-md-auto form-inline">
                    <div class='LEDPanelsConnectionLabel col-md-auto form-inline'><b>Connection Type:</b>

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
                                } else if ($settings['BeaglePlatform']) { ?>
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
                                <? } else if (strpos($settings['SubPlatform'], 'PocketBeagle2') !== false) { ?>
                                            <option value='PocketScroller1x'>PocketScroller</option>
                                            <option value='PocketScroller3x'>PocketScroller v3</option>
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




                </div>

                <div class="col-md-auto ms-lg-auto form-inline">
                    <div class="form-actions">
                        <input id="RemovePanelMatrixButton" type='button' class="buttons ms-1 btn-danger"
                            onClick='RemovePanelMatrixConfig();' value='Remove Panel Matrix'>
                    </div>
                </div>


                <div class="col-md-auto form-inline">
                    <b>Matrix Name:</b>
                    <input class='LEDPanelMatrixName' type='text' onchange="MatrixNameChange()">
                    <span id="panelMatrixName_tip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto"
                        data-bs-title="This setting is purely cosmetic to allow you to give each Matrix a distinctive name"><img
                            id="panelMatrixName_img" src="images/redesign/help-icon.svg" class="icon-help"
                            alt="panelMatrixNameHelp icon"></span>
                </div>
            </div>
            <div class='divLEDPanelsData'>
                <div class="container-fluid settingsTable">
                    <h3>Matrix Settings:</h3>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2">
                            <span class='ledPanelSimpleUI'><b>Panel Layout:</b></span>
                            <span class='ledPanelCanvasUI'><b>Matrix Size (WxH):</b></span>
                        </div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
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
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
                            <input class='LEDPanelsStartChannel' type=number min=1 max=<?= FPPD_MAX_CHANNELS ?>
                                value='1'>
                        </div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Single Panel Size (WxH):</b></div>
                        <div class="printSettingFieldCola col-md-4 col-lg-4">
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Channel Count:</b></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4"><span
                                class='LEDPanelsChannelCount'>1536</span>(<span class='LEDPanelsPixelCount'>512</span>
                            Pixels)</div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Model Start Corner:</b></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
                            <select class='LEDPanelsStartCorner'>
                                <option value='0'>Top Left</option>
                                <option value='1'>Bottom Left</option>
                            </select>
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><b>Default Panel Color Order (C-Def):</b>
                        </div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
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
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
                            <? printLEDPanelGammaSelect($settings['Platform']); ?>
                        </div>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4"></div>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                class='LEDPanelsBrightnessLabel'><b>Brightness:</b></span></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
                            <select class='LEDPanelsBrightness'>
                                <?
                                if ($settings['Platform'] == "Raspberry Pi") {
                                    for ($x = 100; $x >= 5; $x -= 5) {
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
                            <div class="printSettingFieldCol col-md-4 col-lg-4">
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
                                <div class="printSettingFieldCol col-md-4 col-lg-4"><input class='LEDPanelsOutputByRow'
                                        type='checkbox' onclick='outputByRowClicked()'></div>
                        <? } else { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                                <div class="printSettingFieldCol col-md-4 col-lg-4"></div>
                        <? } ?>

                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                class='LEDPanelInterleaveLabel'><b>Panel Interleave:</b></span></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
                            <? printLEDPanelInterleaveSelect(); ?>
                        </div>
                        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                            <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                    class='LEDPanelsOutputCPUPWMLabel'><b>Use CPU PWM:</b></span></div>
                            <div class="printSettingFieldCol col-md-4 col-lg-4"><input class='LEDPanelsOutputCPUPWM'
                                    type='checkbox'></div>
                        <? } else if ($settings['BeaglePlatform']) { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                        class='LEDPanelsOutputBlankRowLabel'><b>Blank between rows:</b></span></div>
                                <div class="printSettingFieldCol col-md-4 col-lg-4"><input class='LEDPanelsOutputBlankRow'
                                        type='checkbox'></div>
                        <? } else { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                                <div class="printSettingFieldCol col-md-4 col-lg-4"></div>
                        <? } ?>
                    </div>
                    <div class="row">
                        <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                class='LEDPanelsColorDepthLabel'><b>Color Depth:</b></span></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
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
                        <div class="printSettingFieldCol col-md-4 col-lg-4"></div>
                    </div>

                    <div class="row">
                        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                            <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                    class='LEDPanelsRowAddressTypeLabel'><b>Panel
                                        Row Address Type:</b></span></div>
                            <div class="printSettingFieldCol col-md-4 col-lg-4">
                                <select class='LEDPanelsRowAddressType'>
                                    <option value='0' selected>Standard</option>
                                    <option value='1'>AB-Addressed Panels</option>
                                    <option value='2'>Direct Row Select</option>
                                    <option value='3'>ABC-Addressed Panels</option>
                                    <option value='4'>ABC Shift + DE Direct</option>
                                </select>
                            </div>
                        <? } else if (strpos($settings['SubPlatform'], 'PocketBeagle2') !== false) { ?>
                                <div class="printSettingLabelCol col-md-2 col-lg-2"><span
                                        class='LEDPanelsRowAddressTypeLabel'><b>Panel Addressing Type:</b></span></div>
                                <div class="printSettingFieldCol col-md-4 col-lg-4">
                                    <select class="LEDPanelsRowAddressType" onchange="RowAddressTypeChanged();">
                                        <option value='0' selected>Standard</option>
                                        <option value='2'>Direct Row Select</option>
                                        <?
                                        if ($panelCapesDriver == "BBShiftPanel") {
                                            //echo "<option value='50'>FM6353C</option>";
                                            echo "<option value='51'>FM6363C</option>";
                                        }
                                        ?>
                                    </select>
                                </div>
                        <? } else { ?>
                                <div class=" printSettingLabelCol col-md-2 col-lg-2">
                                </div>
                                <div class="printSettingFieldCol col-md-4 col-lg-4"></div>
                        <? } ?>
                    </div>

                    <div class="row">
                        <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                            <div class="printSettingLabelCol col-md-2 col-lg-2"><span class='LEDPanelsTypeLabel'><b>LED
                                        Panel
                                        Type:</b></span></div>
                            <div class="printSettingFieldCol col-md-4 col-lg-4">
                                <select class='LEDPanelsType'>
                                    <option value='0' selected>Standard</option>
                                    <option value='1'>FM6126A</option>
                                    <option value='2'>FM6127</option>
                                </select>
                            </div>
                        <? } else { ?>
                            <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                            <div class="printSettingFieldCol col-md-4 col-lg-4"></div>
                        <? } ?>
                        <div class="printSettingLabelCol col-md-2 col-lg-2"></div>
                        <div class="printSettingFieldCol col-md-4 col-lg-4">
                            <button class="vendorPanelSettingsBtn buttons btn-outline-success btn-rounded">Vendor
                                Panel Properties
                            </button>
                        </div>
                    </div>

                </div>
            </div>
            <div class='divLEDPanelsLayoutData'>
                <div style="padding: 10px;">
                    <h3>LED Panel Layout:</h3>
                    Advanced Layout?
                    <input type="checkbox" onchange="ToggleAdvancedLayout();" class="LEDPanelUIAdvancedLayout">
                    <?// PrintSettingCheckbox("Advanced Layout", "LEDPanelUIAdvancedLayout", 0, 0, "1", "0", "", "ToggleAdvancedLayout", 0); ?>
                    <!-- <script>$('#LEDPanelUIAdvancedLayout').addClass('LEDPanelUIAdvancedLayout').removeAttr("id");</script> -->
                    <br>
                    <span class='ledPanelSimpleUI'>
                        View Config from front?
                        <input type="checkbox" onchange="FrontBackViewToggled();" class="LEDPanelUIFrontView">
                        <? //PrintSettingCheckbox("Front View", "LEDPanelUIFrontView", 0, 0, "1", "0", "", "FrontBackViewToggled", 1); ?>
                        <!--<script>$('#LEDPanelUIFrontView').addClass('LEDPanelUIFrontView').removeAttr("id");</script> -->
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
                                            <style>
                                                .rotate-btn {
                                                    display: flex;
                                                    align-items: center;
                                                    gap: 8px;
                                                    /* Space between icon and text */
                                                    padding: 10px 20px;
                                                    font-size: 16px;
                                                    border: none;
                                                    background-color: rgb(94, 97, 100);
                                                    color: white;
                                                    cursor: pointer;
                                                    border-radius: 5px;
                                                }

                                                .rotate-btn:hover {
                                                    background-color: rgb(133, 134, 134);
                                                }

                                                .icon {
                                                    font-size: 20px;
                                                }
                                            </style>

                                            <td colspan=2>
                                                <button class="rotate-btn" onclick="RotateCanvasPanel();">
                                                    <i class="fas fa-rotate icon"></i>
                                                    Rotate
                                                </button>
                                            </td>
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
                    } else if ($settings['BeaglePlatform']) {
                        if ($panelCapesDriver != "") {
                            echo "<option value='$panelCapesDriver'>Hat/Cap/Cape</option>\n";
                        } else {
                            echo "<option value='LEDscapeMatrix'>Hat/Cap/Cape</option>\n";
                        }
                    }
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

<div id="vendorPanelSelectDialog" class="vendorPanelSelectDialog hidden">
    <div class="form-group">
        <p>These settings are provided either the vendor or users who have tested them. If the settings do not work
            properly, please work with the vendor to determine the proper settings that are required and log an issue or
            pull request.
        </p>
        <label for="selectVendor">Vendor:</label>
        <select id="selectVendor" class="selectVendor form-control" onChange="PopulatePanelTypes();">
            <option value="">Select Vendor</option>
        </select>
        <label for="selectPanel">Panel Type:</label>
        <select id="selectPanel" class="selectPanel form-control" onChange="PanelTypeSelected();">
        </select>
        <div id="vendorPanelURL" class="vendorPanelURL"></div>
    </div>
</div>