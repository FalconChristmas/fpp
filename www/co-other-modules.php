<script>
    /*
    Quick instructions for adding new output types
    If needed extend a class to add input fields on the Output Config column.  Create a new object for the output type and it to the output_modules array at the bottom.  No need to edit co-other.php.
    Name = type, fpp uses this name to load the correct module
    friendlyName = is the display name for the type and is what is displayed in the Output Type column
    maxChannels = is the max channels that can be configured.  It is also the default channel count for a new row.
    fixedStart = true when the output has a fixed start channel.
    fixedChans = true when the output has a fixed channel count.
    */

    /////////////////////////////////////////////////////////////////////////////
    //Base Class

    class OtherBase {
        constructor(name = "Base-Type", friendlyName = "Base Name", maxChannels = 512, fixedStart = false, fixedChans = false, config = {}) {
            this._typeName = name;
            this._typeFriendlyName = friendlyName;
            this._maxChannels = maxChannels;
            this._fixedStart = fixedStart;
            this._fixedChans = fixedChans;
            this._config = config;
        }

        PopulateHTMLRow(config) {
            var results = "";
            return results;
        }

        AddNewRow() {
            return this.PopulateHTMLRow(this._config);
        }

        GetOutputConfig(result, cell) {
            return result;
        }

        SetDefaults(row) {
            if (this._fixedStart)
                row.find("td input.start").prop('disabled', true);

            row.find("td input.count").val(this._maxChannels);
            if (this._fixedChans)
                row.find("td input.count").prop('disabled', true);
        }

        RowAdded(row) {
            if (this._fixedStart)
                row.find("td input.start").prop('disabled', true);

            if (this._fixedChans)
                row.find("td input.count").prop('disabled', true);
        }

        CanAddNewOutput() {
            return true;
        }

        get typeName() {
            return this._typeName;
        }

        get typeFriendlyName() {
            return this._typeFriendlyName;
        }

        get maxChannels() {
            return this._maxChannels;
        }

        get fixedChans() {
            return this._fixedChans;
        }

        get fixedStart() {
            return this._fixedStart;
        }

    }

    /////////////////////////////////////////////////////////////////////////////
    //Base Class for outputs with a "Port" device such as serial and SPI
    class OtherBaseDevice extends OtherBase {
        constructor(name = "Base-Type-Device", friendlyName = "Base Device Name", maxChannels = 512, fixedStart = false, fixedChans = false, devices = ["No Devices"], config = {}) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config)
            this._devices = devices;
        }

        PopulateHTMLRow(config) {
            var results = "";
            results += DeviceSelect(this._devices, config.device) + "&nbsp;";
            return results;
        }

        GetOutputConfig(result, cell) {
            var device = cell.find("select.device").val();

            if (device == "")
                return "";

            result.device = device;
            return result;
        }

        CanAddNewOutput() {
            if (this._devices[0] == "No Devices" || Object.keys(this._devices).length == 0) {
                alert('There are no available devices for this channel output type.');
                return false;
            }
            return true;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // Base Class for outputs which can auto-create a FrameBuffer Pixel Overlay Model
    class OtherAutoFBBaseDevice extends OtherBaseDevice {
        constructor(name = "AutoFB-Base-Type-Device", friendlyName = "AutoFB Base Device Name", maxChannels = 512, fixedStart = false, fixedChans = false, devices = ["No Devices"], config = {}) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, devices, config)

            this._allowInvert = false;
            this._allowPixelSize = false;
            this._allowScaling = false;
            this._updateChannelCount = true;
            this._defaultWidth = 1920;
            this._defaultHeight = 1080;
            this._defaultScaling = 1;
        }

        PopulateHTMLRow(config) {
            var tmpDevices = {};
            tmpDevices[''] = '-- Auto Create --';
            var keys = Object.keys(this._devices);
            for (var i = 0; i < keys.length; i++) {
                tmpDevices[keys[i]] = this._devices[keys[i]];
            }

            // Convert old VirtualDisplay config to new
            if (config.hasOwnProperty('ModelName'))
                config.modelName = config.ModelName;
            if (config.hasOwnProperty('PixelSize'))
                config.pixelSize = config.PixelSize;

            if (!config.hasOwnProperty('modelName'))
                config.modelName = '';

            var result = CreateSelect(tmpDevices, config.modelName, 'Model', '', 'device', 'OtherDeviceSelectedModelChanged(this, ' + (this._updateChannelCount ? 'true' : 'false') + ');') + "&nbsp;";

            if (this._allowPixelSize) {
                result += "Pixel Size:&nbsp;<select class='pixelSize'>";
                for (i = 1; i <= 10; i++) {
                    result += "<option value='" + i + "'";
                    if (config.pixelSize == i)
                        result += " selected";
                    result += ">" + i + "</option>";
                }
                result += "</select>";
            }

            if (this._allowInvert) {
                result += "Invert:&nbsp;<input type='checkbox' class='invert'";
                if (config.invert)
                    result += " checked";
                result += ">";
            }

            var width = this._defaultWidth;
            var height = this._defaultHeight;
            var scaling = this._defaultScaling;
            var device = 'fb0';
            var hidden = "style='display: none;'";

            if (config.modelName == '') {
                if (config.hasOwnProperty('device') && (config.device != '')) {
                    width = config.width;
                    height = config.height;
                    device = config.device;

                    if (!isNaN(parseInt(config.scaling)))
                        scaling = config.scaling;
                }

                hidden = '';
            }

            result += "<span class='fbInfo' " + hidden + "> Device: "
                + CreateSelect(FBDevices, device.replace('/dev/', ''), '', '', 'fbDevice', '')
                + " Size: <input type='number' min='8' max='4096' step='2' class='width' value='" + width + "' onChange='AutoFBDeviceLayoutChanged(this, " + (this._updateChannelCount ? 'true' : 'false') + ");'>"
                + " <b>X</b> <input type='number' min='8' max='2160' step='2' class='height' value='" + height + "' onChange='AutoFBDeviceLayoutChanged(this, " + (this._updateChannelCount ? 'true' : 'false') + ");'>";

            if (this._allowScaling) {
                result += " Pixel Size: <input type='number' min='1' max='64' class='scaling' value='" + scaling + "' onChange='AutoFBDeviceLayoutChanged(this, " + (this._updateChannelCount ? 'true' : 'false') + ");'>";
            } else {
                result += "<input type='hidden' value='1'>";
            }

            result += "</span><img class='cmdTmplTooltipIcon' title='This channel output draws on a Pixel Overlay Model.  You may choose to Auto-Create a Pixel Overlay Model to display on the video output for this device or select from an existing Pixel Overlay Model in the list.' src='images/redesign/help-icon.svg' width=22 height=22 style='float: right;'>";

            return result;
        }

        GetOutputConfig(result, cell) {
            result.modelName = cell.find('select.device').val();
            result.invert = cell.find('input.invert').is(':checked');

            if (result.modelName == '') {
                result.device = cell.find('select.fbDevice').val();
                result.width = parseInt(cell.find('input.width').val());
                result.height = parseInt(cell.find('input.height').val());

                if (this._allowScaling)
                    result.scaling = parseInt(cell.find('input.scaling').val());
            }

            if (this._allowPixelSize)
                result.pixelSize = parseInt(cell.find("select.pixelSize").val());

            return result;
        }

        SetDefaults(row) {
            super.SetDefaults(row);
            if (this._config.modelName != '') {
                $(row).find('input.count').val(PixelOverlayModelChannels[this._config.modelName]);
            } else if (this._updateChannelCount) {
                $(row).find("input.count").val(this._defaultWidth * this._defaultHeight * 3 / this._defaultScaling / this._defaultScaling);
            }
        }

        RowAdded(row) {
            super.RowAdded(row);
            if ($(row).find('.device').val() != '') {
                $(row).find('input.count').val(PixelOverlayModelChannels[$(row).find('.device').val()]);
            } else {
                AutoFBDeviceLayoutChanged($(row).find('.width'));
            }
        }

        CanAddNewOutput() {
            if (Object.keys(PixelOverlayModels).length || Object.keys(FBDevices).length) {
                return true;
            }
            alert('There are no Pixel Overlay Models defined and no FrameBuffer devices available.');
            return false;
        }
    }

    function OtherDeviceSelectedModelChanged(item, updateChannelCount = false) {
        var value = $(item).val();

        if (value != '') {
            $(item).parent().find('.fbInfo').hide();
            if (updateChannelCount)
                $(item).parent().parent().find('input.count').val(PixelOverlayModelChannels[$(item).val()]);
        } else {
            $(item).parent().find('.fbInfo').show();
            AutoFBDeviceLayoutChanged($(item).parent().parent().find('.width'), updateChannelCount);
        }

    }

    function AutoFBDeviceLayoutChanged(item, updateChannelCount = false) {
        var width = $(item).parent().find('.width').val();
        var height = $(item).parent().find('.height').val();
        //var scaling = $(item).parent().find('.scaling').val();
        var scaling = 1;
        var channels = width * height * 3 / scaling / scaling;

        if (updateChannelCount)
            $(item).parent().parent().parent().find('.count').val(channels);
    }


    /////////////////////////////////////////////////////////////////////////////
    // SPI/Serial/FrameBuffer Devices

    var SPIDevices = new Array();
    <?
    $hasSPI = false;
    foreach (scandir("/dev/") as $fileName) {
        if (preg_match("/^spidev[0-9]/", $fileName)) {
            echo "SPIDevices['$fileName'] = '$fileName';\n";
            $hasSPI = true;
        }
    }
    ?>

    var SerialDevices = new Array();
    <?
    if (isset($settings['cape-info']['tty-labels'])) {
        foreach ($settings['cape-info']['tty-labels'] as $label => $device) {
            echo "SerialDevices['$label'] = '$label';\n";
        }
    }
    foreach (scandir("/dev/") as $fileName) {
        if (
            (preg_match("/^ttySC?[0-9]+/", $fileName)) ||
            (preg_match("/^ttyACM[0-9]+/", $fileName)) ||
            (preg_match("/^ttyO[0-9]/", $fileName)) ||
            (preg_match("/^ttyAMA[0-9]+/", $fileName)) ||
            (preg_match("/^ttyUSB[0-9]+/", $fileName))
        ) {
            echo "SerialDevices['$fileName'] = '$fileName';\n";
        }
    }
    if (is_dir("/dev/serial/by-id")) {
        foreach (scandir("/dev/serial/by-id") as $fileName) {
            if (strcmp($fileName, ".") != 0 && strcmp($fileName, "..") != 0) {
                $linkDestination = basename(readlink('/dev/serial/by-id/' . $fileName));
                echo "SerialDevices['serial/by-id/$fileName'] = '$fileName -> $linkDestination';\n";
            }
        }
    }
    ?>

    var I2CDevices = new Array();
    <?
    $hasI2C = false;
    foreach (scandir("/dev/") as $fileName) {
        if (preg_match("/^i2c-[0-9]/", $fileName)) {
            echo "I2CDevices['$fileName'] = '$fileName';\n";
            $hasI2C = true;
        }
    }
    ?>

    var FBDevices = new Array();
    <?
    exec($SUDO . " " . $settings["fppBinDir"] . "/fpp -FB", $output, $return_val);
    $js = json_decode($output[0]);
    foreach ($js as $devname) {
        echo "FBDevices['$devname'] = '$devname';\n";
    }
    ?>

    var PixelOverlayModels = new Array();
    var PixelOverlayModelChannels = new Array();
    <?
    $json = file_get_contents('http://localhost/api/models');
    $models = json_decode($json, true);
    foreach ($models as $model) {
        $modelName = $model['Name'];
        $pixelSize = isset($model['PixelSize']) ? $model['PixelSize'] : 1;
        if ((($model['Type'] == 'FB') || ($model['Type'] == 'Sub')) && ($modelName != 'FB - fb0') && ($modelName != 'FB - fb1')) {
            $width = intval($model['Width'] / $pixelSize);
            $height = intval($model['Height'] / $pixelSize);
            echo "PixelOverlayModels['$modelName'] = '$modelName (" . $width . 'x' . $height . ")';\n";
            echo "PixelOverlayModelChannels['$modelName'] = " . $model['ChannelCount'] . ";\n";
        }
    }
    ?>


    /////////////////////////////////////////////////////////////////////////////
    // WS2801 Output via SPI

    class SPIws2801Device extends OtherBaseDevice {

        constructor() {
            super("SPIws2801", "SPI-WS2801", 1530, false, SPIDevices, { pi36: 0 });
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);

            result += " PI36: <input type=checkbox class='pi36'";
            if (config.pi36)
                result += " checked='checked'";

            result += ">";

            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);

            if (result == "")
                return "";

            var pi36 = 0;

            if (cell.find("input.pi36").is(":checked"))
                pi36 = 1;

            result.pi36 = parseInt(pi36);

            return result;
        }

    }

    /////////////////////////////////////////////////////////////////////////////
    // I2C Output
    class I2COutput extends OtherBaseDevice {

        constructor(name = "I2C-Output", friendlyName = "I2C Output", maxChannels = 512, fixedStart = false, fixedChans = false, config = { deviceID: 0x40 }, minAddr = 0x00, maxAddr = 0x7f) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, I2CDevices, config);
            this._minAddr = minAddr;
            this._maxAddr = maxAddr;
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);

            var addrs = {};
            //build addr array for select option
            for (var addr = this._minAddr; addr <= this._maxAddr; addr++) {
                addrs[addr] = "Hex: 0x" + addr.toString(16) + ", Dec: " + addr;
            }

            result += " " + CreateSelect(addrs, config.deviceID, "I2C Address", "Select Address", "addr");

            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            var addr = cell.find("select.addr").val();
            result.deviceID = parseInt(addr);
            return result;
        }

        CanAddNewOutput() {
            //almost all i2c devices allow setting addresses so multiple can exist
            return true;
        }
    }

    class PCA9685Output extends I2COutput {
        constructor(name = "PCA9685", friendlyName = "PCA9685", maxChannels = 32, fixedStart = false, fixedChans = false, config = { device: "i2c-1", deviceID: 0x40, frequency: 50 }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }
        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);
            var datatypes = ["8 Bit Scaled", "8 Bit Scaled Reversed", "16 Bit Scaled", "16 Bit Scaled Reversed", "8 Bit Absolute", "16 Bit Absolute"];
            var zeroBehaviorTypes = ["Hold", "Normal", "To Center", "Stop PWM"];

            var inMicrosecs = config.asUsec;
            if (inMicrosecs == undefined) {
                inMicrosecs = true;
            }
            result += " Frequency (Hz): <input class='frequency' type='number' min='40' max='1600' value='" + config.frequency + "'/><br><input class='asUsec' type='checkbox' " + (inMicrosecs ? "checked" : "") + ">Min/Max in micro-seconds</input><br>";

            result += "<table>";
            for (var x = 0; x < 16; x++) {
                var type = "Servo"; // default
                if (config.ports && config.ports[x] && config.ports[x].type) {
                    type = config.ports[x].type;
                }
                var min = 1000;
                var max = 2000;
                var center = 1500;
                var dataType = 0;
                var zeroBehavior = 0;
                var description = "";
                var brightness = 100;
                var gamma = 1;

                if (config.ports != undefined && config.ports[x] != undefined) {
                    min = config.ports[x].min;
                    max = config.ports[x].max;
                    if (config.ports[x].center != undefined) {
                        center = config.ports[x].center;
                    }
                    if (config.ports[x].zeroBehavior != undefined) {
                        zeroBehavior = config.ports[x].zeroBehavior;
                    }
                    if (config.ports[x].description != undefined) {
                        description = config.ports[x].description;
                    }
                    if (config.ports[x].brightness != undefined) {
                        brightness = config.ports[x].brightness;
                    }
                    if (config.ports[x].gamma != undefined) {
                        gamma = config.ports[x].gamma;
                    }
                    dataType = config.ports[x].dataType;
                }

                result += "<tr style='outline: thin solid;'><td style='vertical-align:top'>Port " + x + ": </td><td>";
                result += "&nbsp;Type:<select class='type" + x + "'>";
                result += "<option value='LED'" + (type === "LED" ? " selected" : "") + ">LED</option>";
                result += "<option value='Servo'" + (type === "Servo" ? " selected" : "") + ">Servo</option>";
                result += "</select>";

                result += "&nbsp;Description:<input class='description" + x + "' type='text' size=30 maxlength=128 style='width: 6em' value='" + description + "'/>";
                result += "<br><br>LED Only:<br>";
                result += "Brightness: ";
                result += "<select class='brightness" + x + "'>";
                for (i = 100; i >= 5; i -= 5) {
                    let selected = (i == brightness) ? " selected" : "";
                    result += "<option value='" + i + "'" + selected + ">" + i + "%</option>";
                }
                result += "</select>";
                result += " Gamma: <input type='number' class='gamma" + x + "' size='3' value='" + gamma + "' min='0.1' max='5.0' step='0.01'>";
                result += "</div>";
                result += "<br><br>Servo Only:<br>";
                result += "&nbsp;Min&nbsp;Value:<input class='min" + x + "' type='number' min='0' max='4095' style='width: 6em' value='" + min + "'/>";
                result += "&nbsp;Center&nbsp;Value:<input class='center" + x + "' type='number' min='0' max='4095' style='width: 6em' value='" + center + "'/>";
                result += "&nbsp;Max&nbsp;Value:<input class='max" + x + "' type='number' min='0' max='4095' style='width: 6em' value='" + max + "'/><br>";
                result += CreateSelect(datatypes, dataType, "Data Type", "Select Data Type", "dataType" + x) + "&nbsp;";
                result += CreateSelect(zeroBehaviorTypes, zeroBehavior, "Zero Behavior", "Select Zero Behavior", "zeroBehavior" + x);
                result += "</td></tr>";
            }
            result += "</table>";

            return result;
        }
        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            result.frequency = parseInt(cell.find("input.frequency").val());
            result.asUsec = cell.find("input.asUsec").is(":checked") ? 1 : 0;
            result.ports = [];
            for (var x = 0; x < 16; x++) {
                var dt = cell.find("select.dataType" + x).val();
                var zt = cell.find("select.zeroBehavior" + x).val();
                result.ports[x] = {};
                result.ports[x].dataType = parseInt(dt);
                result.ports[x].zeroBehavior = parseInt(zt);
                result.ports[x].min = parseInt(cell.find("input.min" + x).val());
                result.ports[x].max = parseInt(cell.find("input.max" + x).val());
                result.ports[x].center = parseInt(cell.find("input.center" + x).val());
                result.ports[x].description = cell.find("input.description" + x).val();
                result.ports[x].type = cell.find("select.type" + x).val();
                result.ports[x].brightness = parseInt(cell.find("select.brightness" + x).val());
                result.ports[x].gamma = parseFloat(cell.find("input.gamma" + x).val());
            }

            return result;
        }

    }

    class PCF8574Output extends I2COutput {
        constructor(name = "PCF8574", friendlyName = "PCF8574", maxChannels = 8, fixedStart = false, fixedChans = false, config = { device: "i2c-1", deviceID: 0x20, pinOrderingInvert: false, relayNumbering: false }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }
        PopulateHTMLRow(config) {
            var pinModes = ["Simple", "Threshold", "Hysteresis"];

            var result = super.PopulateHTMLRow(config);

            var orderInvert = config.pinOrderingInvert;
            if (orderInvert == undefined) {
                orderInvert = false;
            }

            var relayNumbering = config.relayNumbering;
            if (relayNumbering == undefined) {
                relayNumbering = false;
            }

            result += "<br><input class='pinOrderingInvert' type='checkbox' " + (config.pinOrderingInvert ? "checked" : "") + ">Reverse Pin Output Order</input><br>";
            result += "<br><input class='relayNumbering' type='checkbox' " + (config.relayNumbering ? "checked" : "") + ">Relay Port Numbering (1- based)</input><br><br>";


            result += "<table>";

            for (var x = 0; x < 8; x++) {

                var description = "";
                var invert = 0;
                var pinMode = 0;
                var threshold = 128;
                var hysteresisUpper = 192;
                var hysteresisLower = 64;

                if (config.ports != undefined && config.ports[x] != undefined) {
                    if (config.ports[x].invert != undefined) {
                        invert = config.ports[x].invert;
                    }
                    if (config.ports[x].description != undefined) {
                        description = config.ports[x].description;
                    }
                    if (config.ports[x].pinMode != undefined) {
                        pinMode = config.ports[x].pinMode;
                    }
                    if (config.ports[x].threshold != undefined) {
                        threshold = config.ports[x].threshold;
                    }
                    if (config.ports[x].hysteresisUpper != undefined) {
                        hysteresisUpper = config.ports[x].hysteresisUpper;
                    }
                    if (config.ports[x].hystersisLower != undefined) {
                        hysteresisLower = config.ports[x].hysteresisLower;
                    }
                }

                result += "<tr style='outline: thin solid;'><td style='vertical-align:top'>Port " + (relayNumbering ? x + 1 : x) + ": </td><td>";
                result += "Invert&nbsp;Pin:&nbsp;<input class='pinInvert" + x + "'type='checkbox' min='0' max='255' " + (invert ? "checked" : "") + "/>"
                result += CreateSelect(pinModes, pinMode, "Pin mode", "Select Pin Mode", "pinMode" + x) + "&nbsp;";
                result += "&nbsp;Threshold:<input class='threshold" + x + "' type='number' min='0' max='255' style='width: 6em' value='" + threshold + "'/>";
                result += "&nbsp;Hysteresis On:<input class='hysteresisUpper" + x + "' type='number' min='0' max='255' style='width: 6em' value='" + hysteresisUpper + "'/>";
                result += "&nbsp;Hysteresis Off:<input class='hysteresisLower" + x + "' type='number' min='0' max='254' style='width: 6em' value='" + hysteresisLower + "'/>";
                result += "&nbsp;Description:<input class='description" + x + "' type='text' size=30 maxlength=128 style='width: 6em' value='" + description + "'/>";

                result += "</td></tr>";
            }
            result += "</table>";

            return result;
        }
        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);

            result.pinOrderingInvert = cell.find("input.pinOrderingInvert").is(":checked") ? 1 : 0;
            result.relayNumbering = cell.find("input.relayNumbering").is(":checked") ? 1 : 0;


            result.ports = [];
            for (var x = 0; x < 8; x++) {

                var hysteresisUpper = parseInt(cell.find("input.hysteresisUpper" + x).val());
                var hysteresisLower = parseInt(cell.find("input.hysteresisLower" + x).val());

                if (hysteresisUpper < hysteresisLower) {
                    hysteresisUpper = hysteresisLower + 1;
                }


                result.ports[x] = {};
                result.ports[x].invert = cell.find("input.pinInvert" + x).is(":checked") ? 1 : 0;
                result.ports[x].pinMode = parseInt(cell.find("select.pinMode" + x).val());
                result.ports[x].threshold = parseInt(cell.find("input.threshold" + x).val());
                result.ports[x].hysteresisUpper = hysteresisUpper;
                result.ports[x].hysteresisLower = hysteresisLower;
                result.ports[x].description = cell.find("input.description" + x).val();
            }


            return result;
        }

    }

    /////////////////////////////////////////////////////////////////////////////
    // Generic SPI Output
    class GenericSPIDevice extends OtherBaseDevice {

        constructor(name = "GenericSPI", friendlyName = "Generic SPI", maxChannels = FPPD_MAX_CHANNELS, fixedStart = false, fixedChans = false, devices = SPIDevices, config = { speed: 50 }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, devices, config);
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);
            result += " Speed (khz): <input type='number' name='speed' min='1' max='999999' class='speed' value='" + config.speed + "'>";
            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            var speed = cell.find("input.speed").val();

            if (result == "" || speed == "")
                return "";

            result.speed = speed;

            return result;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // Generic UDP Output
    class GenericUDPDevice extends OtherBase {

        constructor(name = "GenericUDP", friendlyName = "Generic UDP", maxChannels = 1400, fixedStart = false, fixedChans = false, config = { address: "", port: 8080, tokens: "" }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);
            result += "IP: <input type='text' name='address' class='address' value='" + config.address + "'>&nbsp;";
            result += "Port: <input type='number' name='port' min='1' max='65536' class='port' value='" + config.port + "'>&nbsp;";
            result += "Data: <input type='text' class='tokens' name='tokens' value='" + config.tokens + "' size='50' maxLength='256'><br>";
            result += "Data tokens include {data} for channel data, {S1} or {S2} for onebyte or two bit channel count, or 0x00 for hex byte";
            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            var port = cell.find("input.port").val();
            result.port = parseInt(port);
            result.address = cell.find("input.address").val();
            result.tokens = cell.find("input.tokens").val();

            return result;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // LOR Enhanced
    var LOREnhancedSpeeds = new Array();
    LOREnhancedSpeeds["19200"] = "19.2k, 15 pixels max";
    LOREnhancedSpeeds["57600"] = "57.6k, 45 pixels";
    LOREnhancedSpeeds["115400"] = "115.4k, 90 pixels max";
    LOREnhancedSpeeds["500000"] = "500 k, 400 pixels max";
    LOREnhancedSpeeds["1000000"] = "1 Mbps, 800 pixels max";

    class LOREnhanced extends OtherBaseDevice {

        constructor(name = "LOREnhanced", friendlyName = "LOR Enhanced", maxChannels = FPPD_MAX_CHANNELS, fixedStart = false, fixedChans = false, devices = SerialDevices, config = { speed: 500000, units: [] }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, devices, config);
        }

        static CreateUnitConfigRow(uintId, numOfPixels, lorStartPixel, fppStartAddr) {
            var result = "<span class='unitConfig'>"
            result += "Unit Id: <input class='unitId' style='opacity: 1' type='number' value='" + uintId.toString() + "' min='1' max='240' />&nbsp;&nbsp;";
            result += "Pixels: <input class='numOfPixels' style='opacity: 1' type='number' value='" + numOfPixels.toString() + "' min='1' max='170' />&nbsp;&nbsp;";
            result += "LOR Start Pix: <input class='lorStartPixel' style='opacity: 1' type='number' value='" + lorStartPixel.toString() + "' min='1' max='170' />&nbsp;&nbsp;";
            result += "FPP Start Ch: <input class='fppStartAddr' style='opacity: 1' type='number' value='" + fppStartAddr.toString() + "' min='1' max='" + this._maxChannels + "' />&nbsp;&nbsp;";
            result += '<input class="buttons" type="button" value="Remove" onClick="LOREnhanced.RemoveUnit(this);" />';
            result += "<br>";
            result += "</span>"
            return result;
        }

        static RemoveUnit(button) {
            $(button).parent().remove();
        }

        static MaxUnits(units) {
            var maxUnitId = 0;
            var start = 1;
            var numOfPixels = 0;
            units.find("span.unitConfig").each(function () {
                var unitId = parseInt($(this).find("input.unitId").val());
                if (unitId > maxUnitId) {
                    maxUnitId = unitId;
                    start = parseInt($(this).find("input.fppStartAddr").val());
                    numOfPixels = parseInt($(this).find("input.numOfPixels").val());
                }
            });
            return {
                unitId: maxUnitId,
                channels: numOfPixels * 3,
                start: start
            };
        }

        static AddUnit(button) {
            var units = $(button).parent();
            var max = LOREnhanced.MaxUnits(units);
            var newUnit = LOREnhanced.CreateUnitConfigRow(max.unitId + 1, 170, 1, max.channels + max.start);
            units.append(newUnit);
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);

            result += "<br>Speed: <select class='speed'>";

            for (var key in LOREnhancedSpeeds) {
                result += "<option value='" + key + "'";

                if (config.speed == key) {
                    result += " selected";
                }

                // Make 500k the default
                if ((config.speed == "") && (key == "500000")) {
                    result += " selected";
                }

                result += ">" + LOREnhancedSpeeds[key] + "</option>";
            }

            result += "</select>";

            result += "<br>";
            result += "<span class='units'>";
            result += '<input class="buttons" type="button" value="Add Unit" onClick="LOREnhanced.AddUnit(this);" /><br>';

            for (var key in config.units) {
                result += LOREnhanced.CreateUnitConfigRow(config.units[key].unitId, config.units[key].numOfPixels, config.units[key].lorStartPixel, config.units[key].fppStartAddr);
            }

            result += "</span>";

            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            var speed = parseInt(cell.find("select.speed").val());
            if (result == "" || speed == "")
                return "";

            result.speed = speed;

            result.units = [];

            var units = cell.find("span.unitConfig");
            units.each(function () {
                $this = $(this);
                var unitId = $this.find("input.unitId").val();
                var numOfPixels = $this.find("input.numOfPixels").val();
                var lorStartPixel = $this.find("input.lorStartPixel").val();
                var fppStartAddr = $this.find("input.fppStartAddr").val();
                result.units.push({
                    unitId: parseInt(unitId),
                    numOfPixels: parseInt(numOfPixels),
                    lorStartPixel: parseInt(lorStartPixel),
                    fppStartAddr: parseInt(fppStartAddr)
                })
            });
            return result;
        }
    }

    class UDMX extends OtherBase {

        constructor(name = "UDMX", friendlyName = "uDMX", maxChannels = 512, fixedStart = false, fixedChans = false, config = {}) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);

            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);

            return result;
        }
    }


    /////////////////////////////////////////////////////////////////////////////
    // Virtual Display Output
    class VirtualDisplayDevice extends OtherAutoFBBaseDevice {
        constructor(name = "VirtualDisplay", friendlyName = "Virtual Display", maxChannels = FPPD_MAX_CHANNELS,
            fixedStart = true, fixedChans = true,
            config = { modelName: '', pixelSize: 2 }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, PixelOverlayModels, config);

            this._allowPixelSize = true;
            this._updateChannelCount = false;

            if (Object.keys(PixelOverlayModels).length > 1)
                this._config.modelName = Object.keys(PixelOverlayModels)[1];
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // HTTP Virtual Display 3D Output (uses pixel indices instead of 2D coordinates)

    class HTTPVirtualDisplay3D extends OtherBase {
        constructor(name = "HTTPVirtualDisplay3D", friendlyName = "HTTP Virtual Display 3D", maxChannels = FPPD_MAX_CHANNELS,
            fixedStart = true, fixedChans = true,
            config = { updateInterval: 25 }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans);
            this._config = config;
        }

        PopulateHTMLRow(config) {
            var results = "";
            results += "Update Interval (ms): <input class='updateInterval' type='number' min='10' max='100' value='" + (config.updateInterval || 25) + "' size='4'/>";
            results += " <span class='text-muted'>(25ms = 40fps)</span>";
            return results;
        }

        SetDefaults(row) {
            super.SetDefaults(row);
            // Hide channel count - it's auto-detected from virtualdisplaymap
            row.find("td input.count").hide().after("<span class='text-muted'>Auto</span>");
        }

        RowAdded(row) {
            super.RowAdded(row);
            // Hide channel count - it's auto-detected from virtualdisplaymap
            row.find("td input.count").hide().after("<span class='text-muted'>Auto</span>");
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            result["port"] = 32329; // Fixed port
            result["updateInterval"] = parseInt(cell.find("input.updateInterval").val()) || 25;
            return result;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // Virtual Matrix Output

    class VirtualMatrixDevice extends OtherAutoFBBaseDevice {

        constructor(name = "VirtualMatrix", friendlyName = "Virtual Matrix", maxChannels = FPPD_MAX_CHANNELS, fixedStart = false, fixedChans = true,
            config = { modelName: '', invert: false }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, PixelOverlayModels, config);

            this._allowInvert = true;
            this._allowScaling = false;
            this._defaultWidth = 192;
            this._defaultHeight = 108;

            if (Object.keys(PixelOverlayModels).length > 1)
                this._config.modelName = Object.keys(PixelOverlayModels)[1];
        }
    }



    /////////////////////////////////////////////////////////////////////////////
    // GPIO Output

    var GPIOPins = new Map();
    var GPIOPinsByNumber = new Map();
    var PWMPins = new Array();
    <?
    $includeFilters = array(
    );
    $excludeFilters = array(
    );
    if (!isset($settings["showAllOptions"]) || $settings["showAllOptions"] == 0) {
        if (isset($settings['cape-info']['show']) && isset($settings['cape-info']['show']['gpio'])) {
            $includeFilters = $settings['cape-info']['show']['gpio'];
        }
        if (isset($settings['cape-info']['hide']) && isset($settings['cape-info']['hide']['gpio'])) {
            $excludeFilters = $settings['cape-info']['hide']['gpio'];
        }
    }

    $data = file_get_contents('http://127.0.0.1:32322/gpio');
    $gpiojson = json_decode($data, true);
    foreach ($gpiojson as $gpio) {
        $pn = $gpio['pin'];

        $hide = false;
        if (count($includeFilters) > 0) {
            $hide = true;
            foreach ($includeFilters as $value) {
                if (preg_match("/" . $value . "/", $pn) == 1) {
                    $hide = false;
                }
            }
        }
        if (count($excludeFilters) > 0) {
            foreach ($excludeFilters as $value) {
                if (preg_match("/" . $value . "/", $pn) == 1) {
                    $hide = true;
                }
            }
        }

        if (!$hide) {
            echo "GPIOPins.set('" . $pn . "', '" . $pn . " (GPIO " . $gpio["gpioChip"] . "/" . $gpio["gpioLine"] . ")');\n";
        }
        echo "GPIOPinsByNumber.set(" . $gpio['gpioLine'] . ", '" . $gpio['pin'] . "');\n";
        if (isset($gpio['pwm'])) {
            echo "PWMPins['" . $gpio['pin'] . "'] = true;\n";
        }


    }
    ?>

    function GPIOHeaderPinChanged(item) {
        var gpio = item.value;
        var row = item.closest('tr');
        if (PWMPins[gpio] != true) {
            $(row).find("span.pwm-span").hide();
            $(row).find("input.pwm").prop('checked', false);
        } else {
            $(row).find("span.pwm-span").show();
        }
    }

    class GPIOOutputDevice extends OtherBase {
        constructor(name = "GPIO", friendlyName = "GPIO", maxChannels = 1, fixedStart = false, fixedChans = true, config = {}) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);
            var gpio = GPIOPins.keys().next().value;
            var pwm = 0;
            var inverted = 0;
            var description = "";
            if (config.pin != undefined) {
                gpio = config.pin;
            } else if (config.gpio != undefined) {
                gpio = GPIOPinsByNumber.get(config.gpio);
            }
            if (config.pwm != undefined) {
                pwm = config.pwm;
            } else if (config.softPWM != undefined) {
                pwm = config.softPWM;
            }
            if (config.invert != undefined) {
                inverted = config.invert;
            }
            if (config.description != undefined) {
                description = config.description;
            }
            result += "Description:<input class='description' type='text' size=30 maxlength=128 style='width: 6em' value='" + description + "'/>";
            result += "&nbsp;";
            result += CreateSelect(GPIOPins, gpio, "GPIO", "", "gpio", "GPIOHeaderPinChanged(this)");
            result += "\n";
            result += " Inverted: <input type=checkbox class='inverted'";
            if (inverted)
                result += " checked='checked'";
            result += ">\n";
            result += "<span class='pwm-span' ";
            if (PWMPins[gpio] != true) {
                result += " style='display: none;'";
            }
            result += ">PWM: <input type='checkbox' class='pwm'";
            if (pwm)
                result += " checked='checked'";
            result += "></span>\n";
            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            result.pin = cell.find("select.gpio").val();
            result.invert = cell.find("input.inverted").is(":checked") ? 1 : 0;
            result.pwm = cell.find("input.pwm").is(":checked") ? 1 : 0;
            result.description = cell.find("input.description").val();
            return result;
        }
    }

    class GPIO595OutputDevice extends OtherBase {
        constructor(name = "GPIO-595", friendlyName = "GPIO-595", maxChannels = 128, fixedStart = false, fixedChans = false, config = {}) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }

        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);
            var clock = GPIOPins[0];
            var data = GPIOPins[0];
            var latch = GPIOPins[0];
            var description = "";
            if (config.clockPin != undefined) {
                clock = config.clockPin;
            }
            if (config.dataPin != undefined) {
                data = config.dataPin;
            }
            if (config.latchPin != undefined) {
                latch = config.latchPin;
            }
            if (config.description != undefined) {
                description = config.description;
            }
            result += "Description:<input class='description' type='text' size=30 maxlength=128 style='width: 6em' value='" + description + "'/>";
            result += "&nbsp;";
            result += CreateSelect(GPIOPins, clock, "Clock", "", "clock", "");
            result += "&nbsp;";
            result += CreateSelect(GPIOPins, data, "Data", "", "data", "");
            result += "&nbsp;";
            result += CreateSelect(GPIOPins, latch, "Latch", "", "latch", "");
            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            result.clockPin = parseInt(cell.find("select.clock").val());
            result.dataPin = parseInt(cell.find("select.data").val());
            result.latchPin = parseInt(cell.find("select.latch").val());
            result.description = cell.find("input.description").val();
            return result;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // MQTT Output
    function MQTTOutputTypeChanged(item) {
        var channelType = item.value;
        var channels = 3;
        if (channelType == "RGBW") {
            channels = 4;
        } else if (channelType == "Single") {
            channels = 1;
        }

        $(item).parent().parent().find("input.count").val(channels);
    }

    class MQTTOutput extends OtherBase {

        constructor(name = "MQTTOutput", friendlyName = "MQTT", maxChannels = 3, fixedStart = false, fixedChans = true, config = { topic: "", payload: "%R%,%G%,%B%", channelType: "RGB" }) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }

        PopulateHTMLRow(config) {

            var datatypes = {};
            datatypes["RGB"] = "RGB";
            datatypes["RGBW"] = "RGBW";
            datatypes["Single"] = "Single";

            var inType = config.channelType;
            if (inType == undefined) {
                inType = "RGB";
            }

            if (inType == "RGB") {
                super._maxChannels = 3;
            } else if (inType == "RGBW") {
                super._maxChannels = 4;
            }
            else if (inType == "Single") {
                super._maxChannels = 1;
            }
            var result = super.PopulateHTMLRow(config);

            result += "Topic: <input type='text' name='topic' class='topic' value='" + config.topic + "'>&nbsp;";
            result += "Payload: <input type='text' class='payload' name='payload' value='" + config.payload + "'><br>";
            result += CreateSelect(datatypes, inType, "Channel Type", "Channel Type", "channelType", "MQTTOutputTypeChanged(this)");
            result += "</td></tr>"

            return result;
        }

        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            result.topic = cell.find("input.topic").val();
            result.payload = cell.find("input.payload").val();
            result.channelType = cell.find("select.channelType").val();

            if (result.channelType == "RGB") {
                result.channelCount = 3;
            } else if (result.channelType == "RGBW") {
                result.channelCount = 4;
            } else if (result.channelType == "Single") {
                result.channelCount = 1;
            }
            return result;
        }
    }

    <?
    if (file_exists($settings['configDirectory'] . "/commandPresets.json")) {
        $commandPresetsFile = file_get_contents($settings['configDirectory'] . "/commandPresets.json");
        json_decode($commandPresetsFile);
        if (json_last_error() === JSON_ERROR_NONE) {
            echo "var commandPresets = " . $commandPresetsFile . ";\n";
        } else {
            echo "var commandPresets = { \"commands\": [] };\n";
        }
    } else {
        echo "var commandPresets = { \"commands\": [] };\n";
    }
    ?>
    function CreatePesetRow(config) {

        var rest = "<div class='row presetRow justify-content-start'>";
        rest += "<div class='col-sm-2'>&nbsp;</div>";
        rest += "<div class='col-sm-2'><input id='channelValue' type='number' min='1' max='255' value='" + config.value + "'></div>";
        rest += "<div class='col-sm-5 command-select'><select id='presetName'>";
        rest += "<option value='' " + (config.preset == "" ? "selected" : "") + ">--Select Command Preset--</option>";
        var commands = commandPresets.commands;
        for (a in commands) {
            var n = commands[a].name;
            rest += "<option value='" + n + "' " + (config.preset == n ? "selected" : "") + ">" + n + "</option>";
        }
        rest += "</select></div>";
        rest += "<div class='col-sm-2 remove-row'><input class='buttons btn-outline-danger' type='button' value='Remove Row' data-btn-enabled-class='btn-outline-danger' onClick='RemovePresetRow(this)' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto' data-bs-title='Remove Preset Row'></div>";
        rest += "<div class='col-sm-1'></div>";
        rest += "</div>";
        return rest;
    }
    function AddPresetValue(button) {
        var container = button.parentElement.parentElement.parentElement;
        var config = new Object();
        config.value = 1;
        config.preset = "";
        container.innerHTML += CreatePesetRow(config);
    }
    function RemovePresetRow(button) {
        var row = button.parentElement.parentElement;
        row.remove();
    }

    class ControlChannelOutput extends OtherBase {
        constructor(name = "ControlChannel", friendlyName = "Control Channel", maxChannels = 1, fixedStart = false, fixedChans = true, config = {}) {
            super(name, friendlyName, maxChannels, fixedStart, fixedChans, config);
        }
        PopulateHTMLRow(config) {
            var result = super.PopulateHTMLRow(config);
            result += "<div class='container' id='presetRows'>";
            result += "<div class='row'>";
            result += "<div class='col-sm-2'><button id='btnAddPreset' class='buttons btn-outline-success' type='button' value='Add' onclick='AddPresetValue(this);'><i class='fas fa-plus'></i> Add</button></div>"
            result += "<div class='col-sm-2'><b>Channel Value</b></div>";
            result += "<div class='col-sm-5'><b>Preset</b></div>";
            result += "<div class='col-sm-2'></div>";
            result += "<div class='col-sm-1'></div>";
            result += "</div>";
            for (var a in config.values) {
                result += CreatePesetRow(config.values[a]);
            }
            result += "</div>";
            return result;
        }
        GetOutputConfig(result, cell) {
            result = super.GetOutputConfig(result, cell);
            result["values"] = [];
            var rows = cell.find("div.presetRow");
            rows.each(function () {
                var chan = $(this).find('#channelValue');
                var preset = $(this).find("#presetName");
                var chanNum = chan.val();
                if (chanNum != "") {
                    var obj = new Object();
                    obj["preset"] = preset.val();
                    obj["value"] = parseInt(chan.val());
                    result["values"].push(obj);
                }
            });

            return result;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    //populate the output devices
    var output_modules = [];

    //Outputs for all platforms

    if (GPIOPins.size > 0) {
        output_modules.push(new GPIOOutputDevice());
        output_modules.push(new GPIO595OutputDevice());
    }
    output_modules.push(new ControlChannelOutput());
    output_modules.push(new MQTTOutput());
    if (Object.keys(SerialDevices).length > 0) {
        output_modules.push(new LOREnhanced());
    }
    output_modules.push(new UDMX());

    //Outputs for Raspberry Pi or Beagle
    <?
    if ($hasI2C) {
        ?>
        output_modules.push(new I2COutput("MCP23017", "MCP23017", 16, false, false, { deviceID: 0x20 }, 0x20, 0x27));
        output_modules.push(new PCA9685Output());
        output_modules.push(new PCF8574Output());
        <?
    }
    if ($hasSPI) {
        ?>
        output_modules.push(new GenericSPIDevice());
        <?
    }
    ?>
    //Outputs for Raspberry Pi
    <?
    if ($settings['Platform'] == "Raspberry Pi") {
        ?>
        //TODO need to see if these modules could run as is on the BeagleBone
        output_modules.push(new SPIws2801Device());
        <?
    }
    ?>
    output_modules.push(new GenericUDPDevice());
    output_modules.push(new VirtualDisplayDevice());
    output_modules.push(new VirtualMatrixDevice());
    output_modules.push(new HTTPVirtualDisplay3D());

</script>