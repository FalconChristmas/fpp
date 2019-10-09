<script>
/////////////////////////////////////////////////////////////////////////////
//Base Class

class OtherBase {
    constructor (name="Base-Type", friendlyName="Base Name", maxChannels=512, fixedChans=false, config = {}) {
        this._typeName = name;
        this._typeFriendlyName = friendlyName;
        this._maxChannels = maxChannels;
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
        row.find("td input.count").val(this._maxChannels);
        if(this._fixedChans)
            row.find("td input.count").prop('disabled', true);
    }

    CanAddNewOutput() {
        return true;
    }

    get typeName () {
        return this._typeName;
    }

    get typeFriendlyName () {
        return this._typeFriendlyName;
    }

    get maxChannels () {
        return this._maxChannels;
    }

    get fixedChans () {
        return this._fixedChans;
    }
}

/////////////////////////////////////////////////////////////////////////////
//Base Class for outputs with a "Port" device such as serial and SPI
class OtherBaseDevice extends OtherBase {
    constructor (name="Base-Type-Device", friendlyName="Base Device Name", maxChannels=512, fixedChans=false, devices=["No Devices"], config = {}) {
        config.device = "";
        super(name, friendlyName, maxChannels, fixedChans, config)
        this._devices = devices;
    }
    
    PopulateHTMLRow(config) {
        var results = "";
        results += DeviceSelect(this._devices, config.device);
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
        if (this._devices[0] == "No Devices" || Object.keys(this._devices).length == 0)
            return false;
        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Misc. Support functions
function DeviceSelect(deviceArray = ["No Devices"], currentValue) {
	var result = "Port: <select class='device'>";

	if (currentValue == "")
		result += "<option value=''>-- Port --</option>";

	var found = 0;
	for (var key in deviceArray) {
		result += "<option value='" + key + "'";
	
		if (currentValue == key) {
			result += " selected";
			found = 1;
		}

		result += ">" + deviceArray[key] + "</option>";
	}

	if ((currentValue != '') &&
		(found == 0)) {
		result += "<option value='" + currentValue + "'>" + currentValue + "</option>";
	}
	result += "</select>";

	return result;
}

/////////////////////////////////////////////////////////////////////////////
// SPI and Serial Devices

var SPIDevices = new Array();
<?
        foreach(scandir("/dev/") as $fileName)
        {
            if (preg_match("/^spidev[0-9]/", $fileName)) {
                echo "SPIDevices['$fileName'] = '$fileName';\n";
            }
        }
?>

var SerialDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^ttyS[0-9]+/", $fileName)) ||
			(preg_match("/^ttyACM[0-9]+/", $fileName)) ||
			(preg_match("/^ttyO[0-9]/", $fileName)) ||
			(preg_match("/^ttyS[0-9]/", $fileName)) ||
			(preg_match("/^ttyAMA[0-9]+/", $fileName)) ||
			(preg_match("/^ttyUSB[0-9]+/", $fileName))) {
			echo "SerialDevices['$fileName'] = '$fileName';\n";
		}
	}
?>



/////////////////////////////////////////////////////////////////////////////
// WS2801 Output via SPI

class SPIWS2801Device extends OtherBaseDevice {
    
    constructor() {
        super("SPI-WS2801", "SPI-WS2801", 1530, false, SPIDevices, {pi36: 0});
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

var output_modules = [];

//Outputs for all platforms

//Outputs for Raspberry Pi or Beagle
<?
if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black")
{
?>


<?
}
?>
//Outputs for Raspberry Pi
<?
if ($settings['Platform'] == "Raspberry Pi")
{
?>
	output_modules.push(new SPIWS2801Device());
<?
}
?>


</script>