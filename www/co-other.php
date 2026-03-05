<?php include 'co-other-modules.php'; ?>
<script>

    $(document).ready(function () {
        PopulateAddOutputMenu();
        GetChannelOutputs();
    });

    /////////////////////////////////////////////////////////////////////////////
    // Update menu to show/hide Virtual Display links
    function UpdateVirtualDisplayMenu(data) {
        var has2D = false;
        var has3D = false;

        // Check if any HTTPVirtualDisplay outputs are enabled
        if (data && data.channelOutputs) {
            for (var i = 0; i < data.channelOutputs.length; i++) {
                var output = data.channelOutputs[i];
                if (output.enabled && output.type === 'HTTPVirtualDisplay') {
                    has2D = true;
                } else if (output.enabled && output.type === 'HTTPVirtualDisplay3D') {
                    has3D = true;
                }
            }
        }

        // Get the Status dropdown menu (the div.dropdown-menu under the Status/Control nav item)
        var statusMenu = $('#navbarDropdownMenuLinkStatus').next('.dropdown-menu');

        // Remove existing Virtual Display menu items
        statusMenu.find('a[href="virtualdisplaywrapper.php"]').remove();
        statusMenu.find('a[href="virtualdisplaywrapper3d.php"]').remove();

        // Find the Display Testing link to insert after it
        var displayTestingLink = statusMenu.find('a[href="testing.php"]');

        // Add 2D Virtual Display link if enabled
        if (has2D) {
            displayTestingLink.after('<a class="dropdown-item" href="virtualdisplaywrapper.php"><i class="fas fa-tv"></i> 2D Virtual Display</a>');
        }

        // Add 3D Virtual Display link if enabled
        if (has3D) {
            var insertAfter = has2D ? statusMenu.find('a[href="virtualdisplaywrapper.php"]') : displayTestingLink;
            insertAfter.after('<a class="dropdown-item" href="virtualdisplaywrapper3d.php"><i class="fas fa-object-group"></i> 3D Virtual Display</a>');
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // nRF Support functions
    function nRFSpeedSelect(speedArray, currentValue) {
        var result = "Speed: <select class='speed'>";

        if (currentValue == "")
            result += "<option value=''>- Speed -</option>";

        var found = 0;
        for (var key in speedArray) {
            result += "<option value='" + key + "'";

            if (currentValue == key) {
                result += " selected";
                found = 1;
            }

            result += ">" + speedArray[key] + "</option>";
        }

        if ((currentValue != '') &&
            (found == 0)) {
            result += "<option value='" + currentValue + "'>" + currentValue + "</option>";
        }
        result += "</select> ";

        return result;
    }

    /////////////////////////////////////////////////////////////////////////////
    function USBDeviceConfig(config) {
        var result = "";

        var description = "";
        if (config.description != null) {
            description = config.description;
        }

        result += "Description:&nbsp;<input class='description' type='text' size=30 maxlength=128 style='width: 6em' value='" + description + "'/>&nbsp;";
        result += DeviceSelect(SerialDevices, config.device) + "&nbsp;";

        return result;
    }

    function GetUSBOutputConfig(result, cell) {
        $cell = $(cell);
        var value = $cell.find("select.device").val();
        var desc = $cell.find("input.description").val();

        if (value == "" && desc == "")
            return "";

        result.device = value;
        result.description = desc;

        return result;
    }

    function NewUSBConfig() {
        var config = {};

        config.device = "";
        config.description = "";

        return USBDeviceConfig(config);
    }

    /////////////////////////////////////////////////////////////////////////////
    // Generic Serial
    var GenericSerialSpeeds = new Array();
    GenericSerialSpeeds["9600"] = "9600";
    GenericSerialSpeeds["19200"] = "19200";
    GenericSerialSpeeds["38400"] = "38400";
    GenericSerialSpeeds["57600"] = "57600";
    GenericSerialSpeeds["115200"] = "115200";
    GenericSerialSpeeds["230400"] = "230400";
    GenericSerialSpeeds["460800"] = "460800";
    GenericSerialSpeeds["921600"] = "921600";

    function GenericSerialSpeedSelect(currentValue) {
        var result = "Speed: <select class='speed'>";

        for (var key in GenericSerialSpeeds) {
            result += "<option value='" + key + "'";

            if (currentValue == key) {
                result += " selected";
            }

            // Make 9600 the default
            if ((currentValue == "") && (key == "9600")) {
                result += " selected";
            }

            result += ">" + GenericSerialSpeeds[key] + "</option>";
        }

        result += "</select>";

        return result;
    }

    function GenericSerialConfig(config) {
        var result = "";

        result += USBDeviceConfig(config);
        result += GenericSerialSpeedSelect(config.speed);
        result += " Header: <input type='text' size=10 maxlength=20 class='serialheader' value='" + config.header + "'>";
        result += " Footer: <input type='text' size=10 maxlength=20 class='serialfooter' value='" + config.footer + "'>";

        return result;
    }

    function NewGenericSerialConfig() {
        var config = {};

        config.description = "";
        config.device = "";
        config.speed = 9600;
        config.header = "";
        config.footer = "";

        return GenericSerialConfig(config);
    }

    function GetGenericSerialConfig(result, cell) {
        $cell = $(cell);
        var device = $cell.find("select.device").val();

        if (device == "")
            return "";

        var speed = $cell.find("select.speed").val();

        if (speed == "")
            return "";

        var desc = $cell.find("input.description").val();
        result.description = desc;

        var header = $cell.find("input.serialheader").val();
        var footer = $cell.find("input.serialfooter").val();

        result.device = device;
        result.speed = parseInt(speed);
        result.header = header;
        result.footer = footer;

        return result;
    }

    /////////////////////////////////////////////////////////////////////////////
    // USB Relay output
    function USBRelayLayoutChanged(item) {
        var channels = parseInt($(item).val());
        $(item).parent().parent().find("input.count").val(channels);
    }

    function USBRelaySubTypeSelect(currentValue) {
        var result = "";
        var options = "Bit,ICStation,CH340".split(",");

        result += " Type:&nbsp;<select class='subType'>";

        var i = 0;
        for (i = 0; i < options.length; i++) {
            var opt = options[i];

            result += "<option value='" + opt + "'";
            if (currentValue == opt)
                result += " selected";
            result += ">" + opt + "</option>";
        }

        result += "</select>  ";

        return result;
    }

    function USBRelayCountSelect(currentValue) {
        var result = "";
        var options = "1,2,4,8".split(",");

        result += " Count:&nbsp;<select class='relayCount' onChange='USBRelayLayoutChanged(this);'>";

        var i = 0;
        for (i = 0; i < options.length; i++) {
            var opt = options[i];

            result += "<option value='" + opt + "'";
            if (currentValue == opt)
                result += " selected";
            result += ">" + opt + "</option>";
        }

        result += "</select>  ";

        return result;
    }

    function USBRelayConfig(config) {
        var result = "";

        result += DeviceSelect(SerialDevices, config.device) + "  ";
        result += USBRelaySubTypeSelect(config.subType);
        result += USBRelayCountSelect(config.relayCount);

        return result;
    }

    function NewUSBRelayConfig() {
        var config = {};

        config.device = "";
        config.subType = "ICStation";
        config.relayCount = 2;

        return USBRelayConfig(config);
    }

    function GetUSBRelayOutputConfig(result, cell) {
        $cell = $(cell);
        var value = $cell.find("select.device").val();

        if (value == "")
            return "";

        result.device = value;

        value = $cell.find("select.subType").val();

        if (value == "")
            return "";

        result.subType = value;

        value = $cell.find("select.relayCount").val();

        if (value == "")
            return "";

        result.relayCount = parseInt(value);

        return result;
    }

    /////////////////////////////////////////////////////////////////////////////
    function NewHTTPVirtualDisplayConfig() {
        var config = {};

        config.width = 1280;
        config.height = 1024;
        config.pixelSize = 2;
        return HTTPVirtualDisplayConfig(config);
    }
    function HTTPVirtualDisplayConfig(config) {
        var result = "";

        result += "Width: <input type=text class='width' size=4 maxlength=4 value='" + config.width + "'> ";
        result += "Height: <input type=text class='height' size=4 maxlength=4 value='" + config.height + "'> ";
        result += "Pixel Size: <select class='pixelSize'>";
        for (i = 1; i <= 10; i++) {
            result += "<option value='" + i + "'";
            if (config.pixelSize == i)
                result += " selected";
            result += ">" + i + "</option>";
        }
        result += "</select>";
        return result;
    }

    function GetHTTPVirtualDisplayConfig(result, cell) {
        $cell = $(cell);

        var width = $cell.find("input.width").val();
        if (width == "")
            return "";

        result.width = parseInt(width);

        var height = $cell.find("input.height").val();
        if (height == "")
            return "";

        result.height = parseInt(height);

        var pixelSize = $cell.find("select.pixelSize").val();
        if (pixelSize == "")
            pixelSize = "1";
        result.pixelSize = parseInt(pixelSize);
        return result;
    }

    /////////////////////////////////////////////////////////////////////////////
    // MAX7219Matrix (MAX7219 w/ 8x8 LED Panels)
    function MAX7219MatrixChanged(item) {
        var panels = parseInt($(item).parent().parent().find("input.panels").val());
        var cpp = parseInt($(item).parent().parent().find("select.channelsPerPixel").val());

        var channels = panels * 64 * cpp;

        $(item).parent().parent().find("input.count").val(channels);
    }

    function NewMAX7219MatrixConfig() {
        var config = {};

        config.panels = 1;
        config.channelsPerPixel = 1;

        return MAX7219MatrixConfig(config);
    }

    function MAX7219MatrixConfig(config) {
        var result = "";

        result += "Panels: <input type=text class='panels' size=2 maxlength=2 value='" + config.panels + "' onChange='MAX7219MatrixChanged(this);'> ";
        result += "Channels Per Pixel: <select class='channelsPerPixel' onChange='MAX7219MatrixChanged(this);'>";
        result += "<option value='1'>1</option>";
        result += "<option value='3'";
        if (config.channelsPerPixel == 3)
            result += " selected";
        result += ">3</option></select>";

        return result;
    }

    function GetMAX7219MatrixConfig(result, cell) {
        $cell = $(cell);

        var panels = $cell.find("input.panels").val();
        if (panels == "")
            return "";

        result.panels = parseInt(panels);

        var channelsPerPixel = $cell.find("select.channelsPerPixel").val();

        if (channelsPerPixel == "")
            return "";

        result.channelsPerPixel = parseInt(channelsPerPixel);

        return result;
    }

    /////////////////////////////////////////////////////////////////////////////
    // Renard Serial Outputs
    var RenardSpeeds = new Array();
    RenardSpeeds["19200"] = "19200";
    RenardSpeeds["38400"] = "38400";
    RenardSpeeds["57600"] = "57600";
    RenardSpeeds["115200"] = "115200";
    RenardSpeeds["230400"] = "230400";
    RenardSpeeds["460800"] = "460800";
    RenardSpeeds["921600"] = "921600";

    function RenardSpeedSelect(currentValue) {
        var result = "Speed: <select class='renardspeed'>";

        for (var key in RenardSpeeds) {
            result += "<option value='" + key + "'";

            if (currentValue == key) {
                result += " selected";
            }

            // Make 57.6k the default
            if ((currentValue == "") && (key == "57600")) {
                result += " selected";
            }

            result += ">" + RenardSpeeds[key] + "</option>";
        }

        result += "</select>";

        return result;
    }

    var RenardParms = new Array();
    RenardParms["8N1"] = "8N1";
    RenardParms["8N2"] = "8N2";

    function RenardParmSelect(currentValue) {
        var result = " <select class='renardparm'>";

        for (var key in RenardParms) {
            result += "<option value='" + key + "'";

            if (currentValue == key) {
                result += " selected";
            }

            // Make 8N1 the default
            if ((currentValue == "") && (key == "8N1")) {
                result += " selected";
            }

            result += ">" + RenardParms[key] + "</option>";
        }

        result += "</select>";

        return result;
    }

    function RenardOutputConfig(config) {
        var result = "";

        result += USBDeviceConfig(config);
        result += RenardSpeedSelect(config.renardspeed);
        result += RenardParmSelect(config.renardparm);

        return result;
    }

    function GetRenardOutputConfig(result, cell) {
        $cell = $(cell);
        var value = $cell.find("select.device").val();
        var desc = $cell.find("input.description").val();

        if (value == "" && desc == "")
            return "";

        result.device = value;
        result.description = desc;

        value = $cell.find("select.renardspeed").val();

        if (value == "")
            return "";

        result.renardspeed = parseInt(value);

        value = $cell.find("select.renardparm").val();

        if (value == "")
            return "";

        result.renardparm = value;

        return result;
    }

    function NewRenardConfig() {
        var config = {};

        config.device = "";
        config.renardspeed = "";
        config.renardparm = "";
        config.description = "";

        return RenardOutputConfig(config);
    }

    /////////////////////////////////////////////////////////////////////////////
    // LOR Serial Outputs
    var LORSpeeds = new Array();
    LORSpeeds["9600"] = "9600";
    LORSpeeds["19200"] = "19200";
    LORSpeeds["38400"] = "38400";
    LORSpeeds["57600"] = "57600";
    LORSpeeds["115200"] = "115200";

    function LORSpeedSelect(currentValue) {
        var result = "Speed: <select class='speed'>";

        for (var key in LORSpeeds) {
            result += "<option value='" + key + "'";

            if (currentValue == key) {
                result += " selected";
            }

            // Make 19.2k the default
            if ((currentValue == "") && (key == "19200")) {
                result += " selected";
            }

            result += ">" + LORSpeeds[key] + "</option>";
        }

        result += "</select>";

        return result;
    }

    function LOROutputConfig(config) {
        var result = "";

        result += USBDeviceConfig(config);
        result += LORSpeedSelect(config.speed);

        var val = config.firstControllerId;
        if (!val) {
            val = 1;
        }
        result += "&nbsp;First Controller ID: <input class='firstControllerId' style='opacity: 1' id='firstControllerId' type='number' value='" + val + "' min='1' max='240' />";
        return result;
    }

    function GetLOROutputConfig(result, cell) {
        $cell = $(cell);
        var value = $cell.find("select.device").val();
        var desc = $cell.find("input.description").val();

        if (value == "" && desc == "")
            return "";

        result.device = value;
        result.description = desc;

        value = $cell.find("select.speed").val();

        if (value == "")
            return "";

        result.speed = parseInt(value);

        value = $cell.find("input.firstControllerId");
        var v2 = parseInt(value.val());

        result.firstControllerId = v2;

        return result;
    }

    function NewLORConfig() {
        var config = {};

        config.device = "";
        config.speed = "";
        config.description = "";

        return LOROutputConfig(config);
    }

    /////////////////////////////////////////////////////////////////////////////
    // nRF/Komby
    var nRFSpeeds = ["250K", "1M"];

    function SPInRFDeviceConfig(config) {
        var result = "";

        result += nRFSpeedSelect(nRFSpeeds, config.speed);
        result += "Channel: <input class='channel' type='text' size=4 maxlength=4 value='" + config.channel + "'>";

        return result;
    }

    function GetnRFSpeedConfig(result, cell) {
        $cell = $(cell);
        var value = $cell.find("select.speed").val();

        if (value == "")
            return "";

        result.speed = parseInt(value);

        var channel = $cell.find("input.channel").val();
        if (!channel && (channel < 0 || channel > 125))
            return "";

        result.channel = parseInt(channel);

        return result;
    }

    function NewnRFSPIConfig() {
        var config = {};

        config.speed = "";
        config.channel = "";

        return SPInRFDeviceConfig(config);
    }


    /////////////////////////////////////////////////////////////////////////////
    // Category Tab Management
    /////////////////////////////////////////////////////////////////////////////
    var categoryTableInfo = {};
    var activeCategoryTabs = {};

    function GetCategoryForType(type) {
        return typeToCategoryMap[type] || "Misc";
    }

    function PopulateAddOutputMenu() {
        var menu = $('#addOutputCategoryMenu');
        menu.empty();
        var orderedCategories = ["DMXSerial", "GPIO", "VirtualDisplay", "SPI", "PWM", "ControlSignal", "ExternalMatrix"];
        for (var i = 0; i < orderedCategories.length; i++) {
            var cat = orderedCategories[i];
            var catInfo = outputCategories[cat];
            if (catInfo && GetTypesForCategory(cat).length > 0 && !activeCategoryTabs[cat]) {
                menu.append('<li><a class="dropdown-item" href="#" onclick="AddNewCategoryTab(\'' +
                    cat + '\'); PopulateAddOutputMenu(); return false;">' + catInfo.label + '</a></li>');
            }
        }
        if (menu.children().length === 0) {
            menu.append('<li><span class="dropdown-item disabled">All groups already added</span></li>');
        }
    }

    function CreateCategoryTab(category) {
        if ($('#tab-other-' + category + '-LI').length > 0) {
            return;
        }
        var catInfo = outputCategories[category];
        if (!catInfo) return;

        var tabLi = '<li class="nav-item" id="tab-other-' + category + '-LI" role="presentation">' +
            '<a class="nav-link" id="tab-other-' + category + '-tab" type="button" ' +
            'data-bs-toggle="pill" data-bs-target="#tab-other-' + category + '" role="tab" ' +
            'aria-controls="tab-other-' + category + '">' + catInfo.label + '</a></li>';
        $('#channelOutputTabs').append(tabLi);

        var tabPane = '<div class="tab-pane fade" id="tab-other-' + category + '" role="tabpanel" ' +
            'aria-labelledby="tab-other-' + category + '-tab">' +
            '<div id="divOther-' + category + '">' +
            '<div class="row tableTabPageHeader">' +
            '<div class="col-md"><h2>' + catInfo.label + ' Outputs</h2></div>' +
            '<div class="col-md-auto ms-lg-auto">' +
            '<div class="form-actions">' +
            '<input id="btnDeleteOther-' + category + '" class="disableButtons" type="button" value="Delete" ' +
            'data-btn-enabled-class="btn-outline-danger" onClick="DeleteOutputFromCategory(\'' + category + '\');">' +
            '<button class="buttons btn-outline-success" type="button" ' +
            'onClick="AddOutputToCategory(\'' + category + '\');"><i class="fas fa-plus"></i> Add</button>' +
            '<input class="buttons btn-success ms-1" type="button" value="Save" ' +
            'onClick="SaveOtherChannelOutputs();">' +
            '</div>' +
            '</div>' +
            '</div>' +
            '<div style="overflow: hidden; padding: 5px;">' +
            '<form>' +
            '<div class="fppTableWrapper">' +
            '<div class="fppTableContents fppFThScrollContainer" role="region" tabindex="0">' +
            '<table id="tblOther-' + category + '" class="fppSelectableRowTable fppStickyTheadTable">' +
            '<thead><tr class="tblheader">' +
            '<th>#</th><th>Active</th><th>Output Type</th>' +
            '<th>Start<br>Ch.</th><th>Channel<br>Count</th><th>Output Config</th>' +
            '</tr></thead>' +
            '<tbody></tbody>' +
            '</table>' +
            '</div>' +
            '</div>' +
            '</form>' +
            '</div>' +
            '</div>' +
            '</div>';

        $('#channelOutputTabsContent').append(tabPane);

        categoryTableInfo[category] = {
            tableName: "tblOther-" + category,
            selected: -1,
            enableButtons: ["btnDeleteOther-" + category],
            disableButtons: []
        };
        SetupSelectableTableRow(categoryTableInfo[category]);

        activeCategoryTabs[category] = true;
    }

    function AddNewCategoryTab(category) {
        CreateCategoryTab(category);
        var tabEl = document.getElementById('tab-other-' + category + '-tab');
        if (tabEl) {
            var tab = new bootstrap.Tab(tabEl);
            tab.show();
        }
    }

    function AddOutputRowToTable(tableId, output, rowIndex) {
        var type = output.type;
        var newRow = "<tr class='rowUniverseDetails'><td style='vertical-align:top'>" + (rowIndex + 1) + "</td>" +
            "<td style='vertical-align:top'><input class='act' type=checkbox";

        if (output.enabled)
            newRow += " checked";

        var startDisabled = "";
        var countDisabled = "";

        let output_module = output_modules.find(obj => obj.typeName == type);

        if (type == 'HTTPVirtualDisplay') {
            startDisabled = " disabled='disabled'";
        }

        if ((type == 'USBRelay') ||
            (type == 'Pixelnet-Lynx') ||
            (type == 'Pixelnet-Open') ||
            (type == 'MAX7219Matrix')) {
            countDisabled = " disabled='disabled'";
        }

        var typeFriendlyName = type;
        if (output_module != undefined) {
            typeFriendlyName = output_module.typeFriendlyName;
            if (output_module._fixedStart) {
                startDisabled = " disabled='disabled'";
            }
            if (output_module._fixedChans) {
                countDisabled = " disabled='disabled'";
            }
        }

        var maxChannel = FPPD_MAX_CHANNELS;
        if ((type == "DMX-Pro") || (type == "DMX-Open")) {
            maxChannel = 512;
        }

        var numChannel = output.channelCount;
        if (output.hasOwnProperty('channelCount')) {
            if (numChannel == 0 || numChannel > maxChannel) {
                numChannel = maxChannel;
            }
        } else {
            numChannel = 1;
        }

        newRow += "></td>" +
            "<td class='type' style='vertical-align:top'>" + typeFriendlyName +
            "<input class='type' type='hidden' name='type' value='" + type + "'></td>" +
            "<td style='vertical-align:top'><input class='start' type=number min=1 max=" + FPPD_MAX_CHANNELS +
            " value='" + output.startChannel + "'" + startDisabled + "></td>" +
            "<td style='vertical-align:top'><input class='count' type=number min=1 max=" + maxChannel +
            " value='" + numChannel + "'" + countDisabled + "></td>" +
            "<td style='vertical-align:top' class='config'>";

        if ((type == "DMX-Pro") ||
            (type == "DMX-Open") ||
            (type == "Pixelnet-Lynx") ||
            (type == "Pixelnet-Open")) {
            newRow += USBDeviceConfig(output);
        } else if (type == "GenericSerial") {
            newRow += GenericSerialConfig(output);
        } else if (type == "Renard") {
            newRow += RenardOutputConfig(output);
        } else if (type == "LOR") {
            newRow += LOROutputConfig(output);
        } else if (type == "SPI-nRF24L01") {
            newRow += SPInRFDeviceConfig(output);
        } else if (type == "HTTPVirtualDisplay") {
            newRow += HTTPVirtualDisplayConfig(output);
        } else if (type == "MAX7219Matrix") {
            newRow += MAX7219MatrixConfig(output);
        } else if (type == "USBRelay") {
            newRow += USBRelayConfig(output);
        } else if (output_module != undefined) {
            newRow += output_module.PopulateHTMLRow(output);
        }

        newRow += "</td></tr>";

        $('#' + tableId + ' > tbody').append(newRow);

        if (output_module != undefined)
            output_module.RowAdded($('#' + tableId + ' > tbody > tr').last());
    }

    /////////////////////////////////////////////////////////////////////////////
    // Channel Output table population and management
    function PopulateChannelOutputTable(data) {
        // Remove all existing category tabs
        var catsToRemove = Object.keys(activeCategoryTabs);
        for (var i = 0; i < catsToRemove.length; i++) {
            var cat = catsToRemove[i];
            $('#tab-other-' + cat + '-LI').remove();
            $('#tab-other-' + cat).remove();
            delete categoryTableInfo[cat];
        }
        activeCategoryTabs = {};

        if (!data || !("channelOutputs" in data) || data.channelOutputs.length == 0) {
            UpdateVirtualDisplayMenu(data);
            return;
        }

        // Group outputs by category
        var grouped = {};
        for (var i = 0; i < data.channelOutputs.length; i++) {
            var output = data.channelOutputs[i];
            var cat = GetCategoryForType(output.type);
            if (!grouped[cat]) grouped[cat] = [];
            grouped[cat].push(output);
        }

        // Create tabs and populate for each category
        for (var cat in grouped) {
            CreateCategoryTab(cat);
            var tableId = 'tblOther-' + cat;
            for (var j = 0; j < grouped[cat].length; j++) {
                AddOutputRowToTable(tableId, grouped[cat][j], j);
            }
        }

        UpdateVirtualDisplayMenu(data);
        PopulateAddOutputMenu();
    }

    function GetChannelOutputs() {
        $.getJSON("api/channel/output/co-other", function (data) {
            PopulateChannelOutputTable(data);
        });
    }

    function SaveOtherChannelOutputs() {
        var postData = {};
        var dataError = 0;
        var rowNumber = 1;
        var config;
        var outputs = [];

        // Remember which tab is active for restore after save
        var activeTabId = $('#channelOutputTabs .nav-link.active').attr('id');

        // Collect from all category tables
        for (var cat in activeCategoryTabs) {
            $('#tblOther-' + cat + ' > tbody > tr.rowUniverseDetails').each(function () {
                $this = $(this);

                var enabled = 0;

                if ($this.find("td:nth-child(2) input.act").is(":checked")) {
                    enabled = 1;
                }

                // Type
                var type = $this.find("input.type").val();

                // User has not selected a type yet
                if (type.indexOf("None Selected") >= 0) {
                    DialogError("Save Channel Outputs",
                        "Output type must be selected on row " + rowNumber);
                    dataError = 1;
                    return;
                }

                var channelCount = $this.find("td:nth-child(5) input").val();
                if ((channelCount == "") || isNaN(channelCount) || (parseInt(channelCount) > FPPD_MAX_CHANNELS)) {
                    DialogError("Save Channel Outputs",
                        "Invalid Channel Count '" + channelCount + "' on row " + rowNumber);
                    dataError = 1;
                    return;
                }

                var startChannel = $this.find("td:nth-child(4) input").val();
                if ((startChannel == "") || isNaN(startChannel)) {
                    DialogError("Save Channel Outputs",
                        "Invalid Start Channel '" + startChannel + "' on row " + rowNumber);
                    dataError = 1;
                    return;
                }

                var endChannel = parseInt(startChannel) + parseInt(channelCount) - 1;
                if (endChannel > FPPD_MAX_CHANNELS) {
                    DialogError("Save Channel Outputs",
                        "Start Channel '" + startChannel + "' plus Channel Count '" + channelCount +
                        "' exceeds " + FPPD_MAX_CHANNELS + " on row " + rowNumber);
                    dataError = 1;
                    return;
                }

                config = {};
                config.enabled = enabled;
                config.type = type;
                config.startChannel = parseInt(startChannel);
                config.channelCount = parseInt(channelCount);

                // Get output specific options
                var maxChannels = 510;
                if ((type == "DMX-Pro") ||
                    (type == "DMX-Open") ||
                    (type == "Pixelnet-Lynx") ||
                    (type == "Pixelnet-Open")) {
                    config = GetUSBOutputConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid Output Config");
                        return;
                    }
                    maxChannels = 512;
                    if (type.substring(0, 8) == "Pixelnet")
                        maxChannels = 4096;
                } else if (type == "Renard") {
                    config = GetRenardOutputConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid Renard Config");
                        return;
                    }
                    maxChannels = 1528;
                } else if (type == "LOR") {
                    config = GetLOROutputConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid LOR Config");
                        return;
                    }
                    maxChannels = 3840;
                } else if (type == "SPI-nRF24L01") {
                    config = GetnRFSpeedConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid nRF Config");
                        return;
                    }
                    maxChannels = 512;
                } else if (type == "HTTPVirtualDisplay") {
                    config = GetHTTPVirtualDisplayConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid HTTPVirtual Display Config");
                        return;
                    }
                    maxChannels = FPPD_MAX_CHANNELS;
                } else if (type == "MAX7219Matrix") {
                    config = GetMAX7219MatrixConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid MAX7219Matrix Config");
                        return;
                    }
                    maxChannels = 1536;
                } else if (type == "GenericSerial") {
                    config = GetGenericSerialConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid Generic Serial Config");
                        return;
                    }
                    maxChannels = 3072;
                } else if (type == "USBRelay") {
                    config = GetUSBRelayOutputConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid USBRelay Config");
                        return;
                    }
                    maxChannels = 8;
                } else if (output_modules.find(obj => obj.typeName == type) != undefined) {
                    ///////new method
                    let output_module = output_modules.find(obj => obj.typeName == type);
                    config = output_module.GetOutputConfig(config, $this.find("td:nth-child(6)"));
                    if (config == "") {
                        dataError = 1;
                        DialogError("Save Channel Outputs", "Invalid " + output_module.typeFriendlyName + " Config");
                        return;
                    }
                    maxChannels = output_module.maxChannels;
                }

                outputs.push(config);
                rowNumber++;
            });

            if (dataError)
                return;
        }

        if (dataError)
            return;

        postData.channelOutputs = outputs;

        $.post("api/channel/output/co-other", JSON.stringify(postData)).done(function (data) {
            PopulateChannelOutputTable(data);
            $.jGrowl("Channel Output Configuration Saved", { themeState: 'success' });
            common_ViewPortChange();

            // Restore active tab if it was a category tab
            if (activeTabId && activeTabId.startsWith('tab-other-')) {
                var tabEl = document.getElementById(activeTabId);
                if (tabEl) {
                    var tab = new bootstrap.Tab(tabEl);
                    tab.show();
                }
            }

            // Update menu to show/hide Virtual Display links
            UpdateVirtualDisplayMenu(data);

            // Auto-disable testing mode when output configs change
            if (typeof disableTestModeIfActive === 'function') {
                disableTestModeIfActive();
            }
        }).fail(function () {
            DialogError("Save Channel Outputs", "Save Failed");
        });
    }

    /////////////////////////////////////////////////////////////////////////////
    // Get available output types for a specific category
    function GetTypesForCategory(category) {
        var types = [];

        // Legacy types for DMX/Serial
        if (category == "DMXSerial") {
            if (Object.keys(SerialDevices).length > 0) {
                types.push({ value: "DMX-Open", label: "DMX-Open" });
                types.push({ value: "DMX-Pro", label: "DMX-Pro" });
                types.push({ value: "GenericSerial", label: "Generic Serial" });
                types.push({ value: "Pixelnet-Lynx", label: "Pixelnet-Lynx" });
                types.push({ value: "Pixelnet-Open", label: "Pixelnet-Open" });
                types.push({ value: "Renard", label: "Renard" });
                types.push({ value: "LOR", label: "LOR" });
            }
        }

        // Legacy types for Virtual Display
        if (category == "VirtualDisplay") {
            types.push({ value: "HTTPVirtualDisplay", label: "HTTP Virtual Display" });
        }

        <?
        if ($settings['Platform'] == "Raspberry Pi") {
            ?>
            // Legacy SPI types (Raspberry Pi only)
            if (category == "SPI") {
                if (Object.keys(SPIDevices).length > 0) {
                    types.push({ value: "SPI-nRF24L01", label: "SPI-nRF24L01" });
                }
            }
            // Legacy External Matrix types (Raspberry Pi only)
            if (category == "ExternalMatrix") {
                if (Object.keys(SPIDevices).length > 0) {
                    types.push({ value: "MAX7219Matrix", label: "MAX7219 Matrix" });
                }
            }
            <?
        }
        ?>

        // Legacy USB Relay for Control Signal
        if (category == "ControlSignal" && Object.keys(SerialDevices).length > 0) {
            types.push({ value: "USBRelay", label: "USBRelay" });
        }

        // Module types for this category
        output_modules.forEach(function (m) {
            if (typeToCategoryMap[m.typeName] == category) {
                types.push({ value: m.typeName, label: m.typeFriendlyName });
            }
        });

        // Remove duplicates (legacy + module)
        var seen = {};
        types = types.filter(function (t) {
            if (seen[t.value]) return false;
            seen[t.value] = true;
            return true;
        });

        // Sort alphabetically
        types.sort(function (a, b) {
            return a.label.localeCompare(b.label);
        });

        return types;
    }

    function AddOtherTypeOptions(row, type) {
        var config = "";

        row.find("td input.start").val("1");
        row.find("td input.start").show();
        row.find("td input.count").show();

        if ((type == "DMX-Pro") ||
            (type == "DMX-Open")) {
            config += NewUSBConfig();
            row.find("td input.count").val("512");
            row.find("td input.count").attr("max", "512");
        } else if ((type == "Pixelnet-Lynx") ||
            (type == "Pixelnet-Open")) {
            config += NewUSBConfig();
            row.find("td input.count").val("4096");
            row.find("td input.count").prop('disabled', true);
        } else if (type == "Renard") {
            config += NewRenardConfig();
            row.find("td input.count").val("286");
        } else if (type == "LOR") {
            config += NewLORConfig();
            row.find("td input.count").val("16");
            row.find("td input.speed").val("19200");
        } else if (type == "SPI-nRF24L01") {
            config += NewnRFSPIConfig();
            row.find("td input.count").val("512");
        } else if (type == "HTTPVirtualDisplay") {
            config += NewHTTPVirtualDisplayConfig();
            row.find("td input.count").val(FPPD_MAX_CHANNELS);
            row.find("td input.count").prop('disabled', true);
            row.find("td input.start").prop('disabled', true);
        } else if (type == "MAX7219Matrix") {
            config += NewMAX7219MatrixConfig();
            row.find("td input.count").val("64");
            row.find("td input.count").prop('disabled', true);
        } else if (type == "GenericSerial") {
            config += NewGenericSerialConfig();
            row.find("td input.count").val("1");
        } else if (type == "USBRelay") {
            config += NewUSBRelayConfig();
            row.find("td input.count").val("2");
            row.find("td input.count").prop('disabled', true);
        } else if (output_modules.find(obj => obj.typeName == type) != undefined) {
            ///////new method
            let output_module = output_modules.find(obj => obj.typeName == type);
            config += output_module.AddNewRow();
            output_module.SetDefaults(row);
        }

        row.find("td:nth-child(6)").html(config);
    }


    function OtherTypeSelected(selectbox) {
        $row = $(selectbox.parentNode.parentNode);

        var type = $(selectbox).val();

        if ((Object.keys(SerialDevices).length == 0) &&
            ((type == 'DMX-Pro') ||
                (type == 'DMX-Open') ||
                (type == 'LOR') ||
                (type == 'Pixelnet-Lynx') ||
                (type == 'Pixelnet-Open') ||
                (type == 'Renard'))) {
            $row.remove();
            return;
        }

        ///////new way
        let output_module = output_modules.find(obj => obj.typeName == type);
        if (output_module != undefined) {
            if (!output_module.CanAddNewOutput()) {
                $row.remove();
                return;
            }
        }

        //add friendly type name if available
        var typeFriendlyName = type;
        if (output_module != undefined)
            typeFriendlyName = output_module.typeFriendlyName;
        $row.find("td.type").html(typeFriendlyName);
        $row.find("td.type").append("<input class='type' type='hidden' name='type' value='" + type + "'>");

        AddOtherTypeOptions($row, type);
    }

    function AddOutputToCategory(category) {
        var tableId = 'tblOther-' + category;
        var selectId = 'outputType-' + category;

        // Check if there's already an unconfigured output
        if ($('#' + selectId).length > 0) {
            DialogError("Configure One Output At a Time",
                "Please configure the newly added output row before adding another");
            return;
        }

        var types = GetTypesForCategory(category);
        if (types.length == 0) {
            DialogError("No Output Types Available",
                "There are no available output types for this category.");
            return;
        }

        var currentRows = $('#' + tableId + ' > tbody > tr').length;

        var newRow =
            "<tr class='rowUniverseDetails'><td style='vertical-align:top'>" + (currentRows + 1) + "</td>" +
            "<td style='vertical-align:top'><input class='act' type=checkbox></td>" +
            "<td style='vertical-align:top' class='type'><select id='" + selectId + "' class='type' onChange='OtherTypeSelected(this);'>" +
            "<option value=''>Select a type</option>";

        for (var i = 0; i < types.length; i++) {
            newRow += "<option value='" + types[i].value + "'>" + types[i].label + "</option>";
        }

        newRow += "</select><input class='type' type='hidden' name='type' value='None Selected'></td>" +
            "<td style='vertical-align:top'><input class='start' type='number' min=1 max=" + FPPD_MAX_CHANNELS + " value='' style='display: none;'></td>" +
            "<td style='vertical-align:top'><input class='count' type='number' min=1 max=" + FPPD_MAX_CHANNELS + " value='' style='display: none;'></td>" +
            "<td> </td>" +
            "</tr>";

        $('#' + tableId + ' > tbody').append(newRow);
    }

    function RenumberColumns(tableName) {
        var id = 1;
        $('#' + tableName + ' > tbody > tr').each(function () {
            $this = $(this);
            $this.find("td:first").html(id);
            id++;
        });
    }

    function DeleteOutputFromCategory(category) {
        var info = categoryTableInfo[category];
        if (!info || info.selected < 0) return;

        $('#tblOther-' + category + ' > tbody > tr:nth-child(' + (info.selected + 1) + ')').remove();
        info.selected = -1;
        SetButtonState("#btnDeleteOther-" + category, "disable");
        RenumberColumns("tblOther-" + category);
    }

</script>